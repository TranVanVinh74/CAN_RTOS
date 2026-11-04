/* USER CODE BEGIN Header */
/*
 * Node 3: CAN sensor reception + A7680C call alert
 *
 * CAN:
 *   500 kbit/s with HSE 8 MHz
 *   PA11 RX, PA12 TX
 *   Standard data frame ID 0x100, DLC 4
 *
 * USART2:
 *   PA2 TX -> modem RXD
 *   PA3 RX <- modem TXD
 *   115200 baud, 8N1
 *
 * Modem powered on manually.
 * No SMS and no actuator command transmission in this version.
 */
/* USER CODE END Header */

#include "main.h"
#include "cmsis_os.h"

/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "A7680C.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
#define ALERT_PHONE             "0399136893"

#define GAS_ALERT_ON            1500U
#define GAS_ALERT_OFF           1000U

#define CAN_DATA_TIMEOUT_MS     3000U
#define CALL_DURATION_MS        30000U
#define CALL_INTERVAL_MS        60000U
#define AT_RETRY_INTERVAL_MS    2000U
/* USER CODE END PD */

/* USER CODE BEGIN PTD */
typedef struct
{
    uint8_t temperature;
    uint8_t humidity;
    uint16_t gas_raw;
    uint32_t received_ms;
} SensorData_t;
/* USER CODE END PTD */

CAN_HandleTypeDef hcan;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/*
 * One-element queue:
 * ISR overwrites it with the newest sensor sample.
 */
static QueueHandle_t xSensorQueue = NULL;
static TaskHandle_t xGatewayTaskHandle = NULL;

volatile SensorData_t g_node3Debug = {0};

/* CAN diagnostics */
volatile uint32_t g_canStage = 0;
volatile uint32_t g_canRxCount = 0;
volatile uint32_t g_canProcessedCount = 0;
volatile uint32_t g_canInvalidCount = 0;
volatile uint32_t g_canReadErrorCount = 0;
volatile uint32_t g_canQueueErrorCount = 0;
volatile uint32_t g_canError = 0;

/* Sensor state */
volatile uint32_t g_sensorSeen = 0;
volatile uint32_t g_sensorFresh = 0;
volatile uint32_t g_sensorAgeMs = 0;
volatile uint32_t g_gasAlarm = 0;

volatile uint32_t g_taskLoops = 0;

/*
 * Call stage:
 * 0  = task not started
 * 1  = initial modem wait
 * 2  = checking AT
 * 3  = modem ready
 * 4  = sending ATD
 * 5  = monitoring call
 * 6  = ATH sent
 * 7  = call terminated by modem
 * 90 = UART error or modem ERROR
 */
volatile uint32_t g_callStage = 0;
volatile uint32_t g_atOK = 0;
volatile uint32_t g_atFailCount = 0;

/*
 * Manual testing:
 * g_callRequest = 1: request one call, respecting cooldown.
 * g_hangupRequest = 1: hang up an active call.
 */
volatile uint32_t g_callRequest = 0;
volatile uint32_t g_hangupRequest = 0;

volatile uint32_t g_callActive = 0;
volatile uint32_t g_callAttempts = 0;
volatile uint32_t g_callElapsedMs = 0;
volatile uint32_t g_callCooldownMs = 0;
volatile uint32_t g_hangupCount = 0;

volatile uint32_t g_uartTxStatus = HAL_OK;
volatile uint32_t g_uartError = 0;

/*
 * Fatal error:
 * 1 oscillator
 * 2 clock
 * 3 UART1
 * 4 UART2
 * 5 CAN init
 * 6 queue allocation
 * 7 task allocation
 * 8 CAN filter
 * 9 CAN start
 * 10 CAN notification
 * 11 scheduler returned
 */
volatile uint32_t g_errorCode = 0;

char g_lastCommand[64];
char g_callRx[256];

static uint16_t callRxLength = 0;
static SensorData_t latestSensor = {0};

static bool sensorSeen = false;
static bool gasAlarm = false;
static bool callActive = false;
static bool callAttempted = false;

static uint32_t callStartedMs = 0;
static uint32_t lastCallAttemptMs = 0;

/* USER CODE END PV */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);

/* USER CODE BEGIN PFP */
static void Task_Gateway(void *argument);
static void CAN_StartReception(void);

static void Sensor_Update(void);
static void Modem_ResetResponse(void);
static void Modem_DrainInput(void);
static bool Modem_Send(const char *command);
static void Modem_ReadByte(void);

static void Call_Start(void);
static void Call_Service(void);
static void Call_Hangup(uint32_t finalStage);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

static void Modem_ResetResponse(void)
{
    memset(g_callRx, 0, sizeof(g_callRx));
    callRxLength = 0;
}

static void Modem_DrainInput(void)
{
    uint8_t byte;
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < 50U)
    {
        if (HAL_UART_Receive(&huart2, &byte, 1, 2) != HAL_OK)
        {
            break;
        }
    }

    __HAL_UART_CLEAR_OREFLAG(&huart2);
}

static bool Modem_Send(const char *command)
{
    HAL_StatusTypeDef status;

    snprintf(g_lastCommand,
             sizeof(g_lastCommand),
             "%s",
             command);

    status = HAL_UART_Transmit(&huart2,
                              (uint8_t *)command,
                              (uint16_t)strlen(command),
                              1000);

    g_uartTxStatus = (uint32_t)status;
    g_uartError = HAL_UART_GetError(&huart2);

    return status == HAL_OK;
}

static void Modem_ReadByte(void)
{
    uint8_t byte;
    HAL_StatusTypeDef status;

    status = HAL_UART_Receive(&huart2, &byte, 1, 20);

    if (status == HAL_OK)
    {
        if (callRxLength >= (sizeof(g_callRx) - 1U))
        {
            memmove(g_callRx,
                    g_callRx + 1,
                    sizeof(g_callRx) - 2U);

            callRxLength = sizeof(g_callRx) - 2U;
        }

        g_callRx[callRxLength++] = (char)byte;
        g_callRx[callRxLength] = '\0';
    }
    else if (status == HAL_ERROR)
    {
        g_uartError = HAL_UART_GetError(&huart2);
        __HAL_UART_CLEAR_OREFLAG(&huart2);
    }
}

static void Sensor_Update(void)
{
    SensorData_t sample;
    uint32_t now = HAL_GetTick();

    if (xQueueReceive(xSensorQueue, &sample, 0) == pdPASS)
    {
        latestSensor = sample;
        sensorSeen = true;

        g_node3Debug = sample;
        g_canProcessedCount++;
    }

    g_sensorSeen = sensorSeen ? 1U : 0U;

    if (!sensorSeen)
    {
        g_sensorFresh = 0;
        g_sensorAgeMs = 0;
        g_gasAlarm = 0;
        return;
    }

    g_sensorAgeMs = now - latestSensor.received_ms;

    g_sensorFresh =
        (g_sensorAgeMs <= CAN_DATA_TIMEOUT_MS) ? 1U : 0U;

    /*
     * Do not use stale readings to arm or clear the alarm.
     * Freshness is also checked before each automatic call.
     */
    if (g_sensorFresh != 0U)
    {
        if (latestSensor.gas_raw > GAS_ALERT_ON)
        {
            gasAlarm = true;
        }
        else if (latestSensor.gas_raw < GAS_ALERT_OFF)
        {
            gasAlarm = false;
        }
    }

    g_gasAlarm = gasAlarm ? 1U : 0U;
}

static void Call_Start(void)
{
    char command[64];

    /*
     * Cooldown applies to every attempt, including failures.
     */
    lastCallAttemptMs = HAL_GetTick();
    callAttempted = true;
    g_callAttempts++;

    g_callRequest = 0;
    g_hangupRequest = 0;
    g_callElapsedMs = 0;

    Modem_DrainInput();
    Modem_ResetResponse();

    snprintf(command,
             sizeof(command),
             "ATD%s;\r\n",
             ALERT_PHONE);

    g_callStage = 4;

    if (!Modem_Send(command))
    {
        g_callStage = 90;
        g_atOK = 0;
        return;
    }

    callStartedMs = HAL_GetTick();
    callActive = true;

    g_callActive = 1;
    g_callStage = 5;
}

static void Call_Hangup(uint32_t finalStage)
{
    /*
     * No long wait here: keep processing CAN in the task loop.
     * The response will remain available to subsequent UART reads.
     */
    if (Modem_Send("ATH\r\n"))
    {
        g_hangupCount++;
        g_callStage = finalStage;
    }
    else
    {
        g_callStage = 90;
        g_atOK = 0;
    }

    callActive = false;
    g_callActive = 0;
    g_hangupRequest = 0;
}

static void Call_Service(void)
{
    g_callElapsedMs = HAL_GetTick() - callStartedMs;

    if (strstr(g_callRx, "NO CARRIER") != NULL ||
        strstr(g_callRx, "BUSY") != NULL ||
        strstr(g_callRx, "NO ANSWER") != NULL ||
        strstr(g_callRx, "NO DIALTONE") != NULL)
    {
        callActive = false;
        g_callActive = 0;
        g_hangupRequest = 0;
        g_callStage = 7;
        return;
    }

    if (strstr(g_callRx, "ERROR") != NULL)
    {
        Call_Hangup(90);
        return;
    }

    if (g_hangupRequest != 0U ||
        g_callElapsedMs >= CALL_DURATION_MS)
    {
        Call_Hangup(6);
    }
}

static void CAN_StartReception(void)
{
    CAN_FilterTypeDef filter = {0};

    g_canStage = 1;

    filter.FilterBank = 0;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;

    /* Accept all, then validate frame in RX callback. */
    filter.FilterIdHigh = 0;
    filter.FilterIdLow = 0;
    filter.FilterMaskIdHigh = 0;
    filter.FilterMaskIdLow = 0;

    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterActivation = ENABLE;
    filter.SlaveStartFilterBank = 14;

    if (HAL_CAN_ConfigFilter(&hcan, &filter) != HAL_OK)
    {
        g_errorCode = 8;
        Error_Handler();
    }

    if (HAL_CAN_Start(&hcan) != HAL_OK)
    {
        g_errorCode = 9;
        Error_Handler();
    }

    /*
     * Queue and scheduler are ready before RX notification.
     */
    if (HAL_CAN_ActivateNotification(
            &hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
    {
        g_errorCode = 10;
        Error_Handler();
    }

    g_canStage = 2;
}

static void Task_Gateway(void *argument)
{
    uint32_t modemWaitStart;
    uint32_t lastATCheckMs = 0;
    uint32_t now;
    uint32_t elapsed;
    bool atChecked = false;
    bool autoRequest;
    bool manualRequest;

    (void)argument;

    A7680C_Init(&huart2,
                A7680C_PWRKEY_GPIO_Port,
                A7680C_PWRKEY_Pin);

    /* Start receiving CAN immediately. */
    CAN_StartReception();

    /*
     * Allow initial modem boot time while servicing CAN.
     * Do not pulse PWRKEY: preserve manual power-on.
     */
    g_callStage = 1;
    modemWaitStart = HAL_GetTick();

    for (;;)
    {
        Sensor_Update();
        g_taskLoops++;
        g_canError = HAL_CAN_GetError(&hcan);

        now = HAL_GetTick();

        if (callAttempted)
        {
            elapsed = now - lastCallAttemptMs;

            g_callCooldownMs =
                (elapsed < CALL_INTERVAL_MS)
                ? (CALL_INTERVAL_MS - elapsed)
                : 0U;
        }
        else
        {
            g_callCooldownMs = 0;
        }

        /*
         * Brief UART read, not a 30-second blocking call loop.
         * CAN is serviced again on the next iteration.
         */
        if (callActive)
        {
            Modem_ReadByte();
            Call_Service();

            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        if ((now - modemWaitStart) < 3000U)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (g_atOK == 0U)
        {
            if (!atChecked ||
                (now - lastATCheckMs) >= AT_RETRY_INTERVAL_MS)
            {
                atChecked = true;
                lastATCheckMs = now;
                g_callStage = 2;

                Modem_DrainInput();

                if (A7680C_IsReady())
                {
                    g_atOK = 1;
                    g_callStage = 3;
                }
                else
                {
                    g_atFailCount++;
                }

                /*
                 * AT checking may take up to one second.
                 * Re-read the latest CAN data before deciding.
                 */
                Sensor_Update();
            }

            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        /*
         * Consume idle modem responses, such as ATH reply.
         */
        Modem_ReadByte();

        /*
         * Refresh sensor age after any UART wait.
         */
        Sensor_Update();

        autoRequest =
            (g_sensorFresh != 0U) &&
            gasAlarm &&
            (latestSensor.gas_raw > GAS_ALERT_ON);

        manualRequest = (g_callRequest == 1U);

        now = HAL_GetTick();

        if ((autoRequest || manualRequest) &&
            (!callAttempted ||
             (now - lastCallAttemptMs) >= CALL_INTERVAL_MS))
        {
            Call_Start();
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/* USER CODE END 0 */

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_CAN_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();

    /* USER CODE BEGIN RTOS_QUEUES */

    /*
     * xQueueOverwriteFromISR requires a one-element queue.
     */
    xSensorQueue = xQueueCreate(1, sizeof(SensorData_t));

    if (xSensorQueue == NULL)
    {
        g_errorCode = 6;
        Error_Handler();
    }

    /* USER CODE END RTOS_QUEUES */

    /* USER CODE BEGIN RTOS_THREADS */

    if (xTaskCreate(Task_Gateway,
                    "CAN_SIM",
                    512,
                    NULL,
                    2,
                    &xGatewayTaskHandle) != pdPASS)
    {
        g_errorCode = 7;
        Error_Handler();
    }

    /* USER CODE END RTOS_THREADS */

    osKernelStart();

    g_errorCode = 11;
    Error_Handler();

    while (1)
    {
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;

    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        g_errorCode = 1;
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct,
                           FLASH_LATENCY_2) != HAL_OK)
    {
        g_errorCode = 2;
        Error_Handler();
    }
}

static void MX_CAN_Init(void)
{
    hcan.Instance = CAN1;

    /* 36 MHz / (4 * (1 + 15 + 2)) = 500 kbit/s. */
    hcan.Init.Prescaler = 4;
    hcan.Init.Mode = CAN_MODE_NORMAL;
    hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan.Init.TimeSeg1 = CAN_BS1_15TQ;
    hcan.Init.TimeSeg2 = CAN_BS2_2TQ;

    hcan.Init.TimeTriggeredMode = DISABLE;
    hcan.Init.AutoBusOff = DISABLE;
    hcan.Init.AutoWakeUp = DISABLE;
    hcan.Init.AutoRetransmission = DISABLE;
    hcan.Init.ReceiveFifoLocked = DISABLE;
    hcan.Init.TransmitFifoPriority = DISABLE;

    if (HAL_CAN_Init(&hcan) != HAL_OK)
    {
        g_errorCode = 5;
        Error_Handler();
    }
}

static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        g_errorCode = 3;
        Error_Handler();
    }
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        g_errorCode = 4;
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Preserve manual modem power-on. */
    GPIO_InitStruct.Pin = A7680C_PWRKEY_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    HAL_GPIO_Init(A7680C_PWRKEY_GPIO_Port, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *can)
{
    CAN_RxHeaderTypeDef header;
    uint8_t data[8];
    SensorData_t sample = {0};
    BaseType_t taskWoken = pdFALSE;

    if (can->Instance != CAN1)
    {
        return;
    }

    while (HAL_CAN_GetRxFifoFillLevel(can, CAN_RX_FIFO0) > 0U)
    {
        if (HAL_CAN_GetRxMessage(can,
                                CAN_RX_FIFO0,
                                &header,
                                data) != HAL_OK)
        {
            g_canReadErrorCount++;
            break;
        }

        if (header.IDE != CAN_ID_STD ||
            header.RTR != CAN_RTR_DATA ||
            header.StdId != 0x100U ||
            header.DLC != 4U)
        {
            g_canInvalidCount++;
            continue;
        }

        sample.temperature = data[0];
        sample.humidity = data[1];

        sample.gas_raw =
            ((uint16_t)data[2] << 8) | (uint16_t)data[3];

        sample.received_ms = HAL_GetTick();

        g_canRxCount++;

        if (xSensorQueue != NULL)
        {
            if (xQueueOverwriteFromISR(xSensorQueue,
                                      &sample,
                                      &taskWoken) != pdPASS)
            {
                g_canQueueErrorCount++;
            }
        }
        else
        {
            g_canQueueErrorCount++;
        }
    }

    portYIELD_FROM_ISR(taskWoken);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4)
    {
        HAL_IncTick();
    }
}

/* USER CODE END 4 */

void Error_Handler(void)
{
    g_canError = HAL_CAN_GetError(&hcan);

    __disable_irq();

    while (1)
    {
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif