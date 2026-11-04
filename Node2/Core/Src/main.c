/* USER CODE BEGIN Header */
/*
 * Node 2: CAN reception + ST7735 display
 *
 * TFT initialization:
 *   main(), before CAN and FreeRTOS scheduler.
 *
 * CAN:
 *   Standard ID 0x100, DLC 4
 *   Byte 0: temperature
 *   Byte 1: humidity
 *   Byte 2: gas high byte
 *   Byte 3: gas low byte
 *
 * Only Task_Actuator updates TFT after scheduler starts.
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "TFT.h"
#include <stdio.h>
/* USER CODE END Includes */

/* USER CODE BEGIN PTD */
typedef struct
{
    uint8_t fan_state;
    uint8_t temperature;
    uint8_t humidity;
    uint16_t gas_level;
} ActuatorCommand_t;
/* USER CODE END PTD */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan;
SPI_HandleTypeDef hspi1;

/* USER CODE BEGIN PV */
QueueHandle_t xActuatorQueue = NULL;
TaskHandle_t xTaskActuatorHandle = NULL;

volatile ActuatorCommand_t g_node2Debug = {0};

/* CAN debug */
volatile uint32_t g_canStage = 0;
volatile uint32_t g_taskLoops = 0;
volatile uint32_t g_canRxCount = 0;
volatile uint32_t g_canProcessedCount = 0;
volatile uint32_t g_canQueueFullCount = 0;
volatile uint32_t g_canInvalidCount = 0;
volatile uint32_t g_canReadErrorCount = 0;
volatile uint32_t g_canError = 0;

/*
 * TFT:
 * 0  = not initialized
 * 1  = initialization in progress
 * 2  = waiting screen drawn
 * 3  = CAN data drawn
 * 99 = SPI error
 */
volatile uint32_t g_tftStage = 0;

/*
 * Fatal error:
 * 0  = no fatal error
 * 1  = oscillator configuration
 * 2  = clock configuration
 * 3  = SPI initialization
 * 4  = CAN initialization
 * 5  = queue allocation
 * 6  = task allocation
 * 7  = scheduler returned unexpectedly
 * 8  = CAN filter configuration
 * 9  = CAN start
 * 10 = CAN notification activation
 */
volatile uint32_t g_errorCode = 0;

/* Defined in TFT.c */
extern volatile uint32_t g_tftSPIErrors;
extern volatile uint32_t g_tftSPIStatus;
extern volatile uint32_t g_tftSPIHalError;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN_Init(void);
static void MX_SPI1_Init(void);

/* USER CODE BEGIN PFP */
void Task_Actuator(void *pvParameters);

static void TFT_DrawInitialScreen(void);
static void TFT_UpdateValues(const ActuatorCommand_t *cmd);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

static void TFT_DrawInitialScreen(void)
{
    g_tftStage = 1;

    /* Initialize exactly once, before scheduler starts. */
    initTFT(&hspi1);

    if (g_tftSPIErrors != 0U)
    {
        g_tftStage = 99;
        return;
    }

    fullDisplay(TFT_BLACK);

    drawString(5, 5, "NODE 2 - CAN",
               Font_7x10, TFT_YELLOW, TFT_BLACK);

    drawLine(0, 22, 127, 22, TFT_WHITE);
drawString(5, 35, "TEMP: --- C",
               Font_7x10, TFT_CYAN, TFT_BLACK);

    drawString(5, 55, "HUM : --- %",
               Font_7x10, TFT_CYAN, TFT_BLACK);

    drawString(5, 75, "GAS : -----",
               Font_7x10, TFT_WHITE, TFT_BLACK);

    drawString(5, 95, "FAN : ---",
               Font_7x10, TFT_WHITE, TFT_BLACK);

    drawString(5, 125, "WAIT CAN... ",
               Font_7x10, TFT_YELLOW, TFT_BLACK);

    g_tftStage = (g_tftSPIErrors == 0U) ? 2U : 99U;
}

static void TFT_UpdateValues(const ActuatorCommand_t *cmd)
{
    char text[24];

    if (g_tftSPIErrors != 0U)
    {
        g_tftStage = 99;
        return;
    }

    /*
     * Fixed field widths overwrite previous digits.
     * Do not clear the whole screen on each CAN message.
     */
    snprintf(text, sizeof(text), "TEMP: %3u C",
             (unsigned int)cmd->temperature);

    drawString(5, 35, text,
               Font_7x10, TFT_CYAN, TFT_BLACK);

    snprintf(text, sizeof(text), "HUM : %3u %%",
             (unsigned int)cmd->humidity);

    drawString(5, 55, text,
               Font_7x10, TFT_CYAN, TFT_BLACK);

    snprintf(text, sizeof(text), "GAS : %5u",
             (unsigned int)cmd->gas_level);

    drawString(5, 75, text,
               Font_7x10, TFT_WHITE, TFT_BLACK);

    if (cmd->fan_state != 0U)
    {
        drawString(5, 95, "FAN : ON ",
                   Font_7x10, TFT_RED, TFT_BLACK);
    }
    else
    {
        drawString(5, 95, "FAN : OFF",
                   Font_7x10, TFT_GREEN, TFT_BLACK);
    }

    drawString(5, 125, "CAN RX OK   ",
               Font_7x10, TFT_GREEN, TFT_BLACK);

    g_tftStage = (g_tftSPIErrors == 0U) ? 3U : 99U;
}

/* USER CODE END 0 */

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_SPI1_Init();

    /* USER CODE BEGIN 2 */

    /*
     * Keep the initialization order that displayed
     * Hello World successfully in your latest test.
     */
    TFT_DrawInitialScreen();

    /* Initialize CAN only after TFT is ready. */
    MX_CAN_Init();

    /* USER CODE END 2 */

    /* USER CODE BEGIN RTOS_QUEUES */
    xActuatorQueue = xQueueCreate(5, sizeof(ActuatorCommand_t));

    if (xActuatorQueue == NULL)
    {
        g_errorCode = 5;
        Error_Handler();
    }
    /* USER CODE END RTOS_QUEUES */

    /* USER CODE BEGIN RTOS_THREADS */
    if (xTaskCreate(Task_Actuator,
                    "CAN_TFT_Task",
                    512,
                    NULL,
                    2,
                    &xTaskActuatorHandle) != pdPASS)
    {
        g_errorCode = 6;
        Error_Handler();
    }
    /* USER CODE END RTOS_THREADS */

    osKernelStart();

    /* USER CODE BEGIN WHILE */
    /* Successful scheduler start does not return. */
    g_errorCode = 7;
    Error_Handler();

    while (1)
    {
    }
    /* USER CODE END WHILE */
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
/* Same clock configuration as your working Node 2. */
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
    /* USER CODE BEGIN CAN_Init 0 */
    /* USER CODE END CAN_Init 0 */

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
        g_canError = HAL_CAN_GetError(&hcan);
        g_errorCode = 4;
        Error_Handler();
    }

    /* USER CODE BEGIN CAN_Init 2 */
    /* USER CODE END CAN_Init 2 */
}

static void MX_SPI1_Init(void)
{
    /* USER CODE BEGIN SPI1_Init 0 */
    /* USER CODE END SPI1_Init 0 */

    /* Same SPI configuration as your working TFT test. */
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;

    /* 72 MHz / 32 = 2.25 MHz. */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;

    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 10;

    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        g_errorCode = 3;
        Error_Handler();
    }

    /* USER CODE BEGIN SPI1_Init 2 */
    /* USER CODE END SPI1_Init 2 */
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

    /* Preserve GPIO configuration from your working file. */
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOB,
                      RELAY_FAN_Pin | TFT_CS_Pin | TFT_RST_Pin,
                      GPIO_PIN_SET);

    HAL_GPIO_WritePin(TFT_DC_GPIO_Port,
                      TFT_DC_Pin,
                      GPIO_PIN_RESET);

    GPIO_InitStruct.Pin =
        RELAY_FAN_Pin |
        TFT_CS_Pin |
        TFT_DC_Pin |
        TFT_RST_Pin;

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* USER CODE BEGIN MX_GPIO_Init_2 */
    /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rxHeader;
    uint8_t rxData[8];
    ActuatorCommand_t cmd = {0};
    BaseType_t taskWoken = pdFALSE;

    if (hcan->Instance != CAN1)
    {
        return;
    }

    /*
     * ISR only reads CAN and queues data.
     * Never draw TFT or call HAL_Delay here.
     */
    while (HAL_CAN_GetRxFifoFillLevel(hcan, CAN_RX_FIFO0) > 0U)
    {
        if (HAL_CAN_GetRxMessage(hcan,
                                CAN_RX_FIFO0,
                                &rxHeader,
                                rxData) != HAL_OK)
        {
            g_canReadErrorCount++;
            break;
        }

        if (rxHeader.IDE != CAN_ID_STD ||
            rxHeader.RTR != CAN_RTR_DATA ||
            rxHeader.StdId != 0x100U ||
            rxHeader.DLC != 4U)
        {
            g_canInvalidCount++;
            continue;
        }

        cmd.temperature = rxData[0];
        cmd.humidity = rxData[1];

        cmd.gas_level =
            ((uint16_t)rxData[2] << 8) |
            (uint16_t)rxData[3];

        /* Display/debug only; no relay GPIO writes. */
        cmd.fan_state = (cmd.gas_level > 1500U) ? 1U : 0U;

        g_canRxCount++;

        if (xActuatorQueue != NULL)
        {
            if (xQueueSendFromISR(xActuatorQueue,
                                 &cmd,
                                 &taskWoken) != pdPASS)
            {
                g_canQueueFullCount++;
            }
        }
    }

    portYIELD_FROM_ISR(taskWoken);
}

void Task_Actuator(void *pvParameters)
{
    CAN_FilterTypeDef filter = {0};
    ActuatorCommand_t cmd;

    (void)pvParameters;

    /*
     * TFT has already been initialized in main().
     * Do not call initTFT() again here.
     */
    g_canStage = 1;

    filter.FilterBank = 0;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;

    /* Receive all frames; validate ID/DLC in callback. */
    filter.FilterIdHigh = 0;
    filter.FilterIdLow = 0;
    filter.FilterMaskIdHigh = 0;
    filter.FilterMaskIdLow = 0;
filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterActivation = ENABLE;
    filter.SlaveStartFilterBank = 14;

    if (HAL_CAN_ConfigFilter(&hcan, &filter) != HAL_OK)
    {
        g_canError = HAL_CAN_GetError(&hcan);
        g_errorCode = 8;
        Error_Handler();
    }

    if (HAL_CAN_Start(&hcan) != HAL_OK)
    {
        g_canError = HAL_CAN_GetError(&hcan);
        g_errorCode = 9;
        Error_Handler();
    }

    /*
     * Queue exists and scheduler is running before
     * enabling CAN RX notification.
     */
    if (HAL_CAN_ActivateNotification(
            &hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
    {
        g_canError = HAL_CAN_GetError(&hcan);
        g_errorCode = 10;
        Error_Handler();
    }

    g_canStage = 2;

    for (;;)
    {
        if (xQueueReceive(xActuatorQueue,
                          &cmd,
                          pdMS_TO_TICKS(1000)) == pdPASS)
        {
            /* Update debug data even if TFT reports an error. */
            g_node2Debug = cmd;
            g_canProcessedCount++;

            TFT_UpdateValues(&cmd);
        }

        g_taskLoops++;
        g_canError = HAL_CAN_GetError(&hcan);
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    /*
     * TIM4 is the HAL timebase in this Node 2 project.
     * Preserve it for HAL_Delay and HAL SPI timeouts.
     */
    if (htim->Instance == TIM4)
    {
        HAL_IncTick();
    }
}

/* USER CODE END 4 */

void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    __disable_irq();

    while (1)
    {
        /* Inspect g_errorCode in Watch. */
    }
    /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    (void)file;
    (void)line;
    /* USER CODE END 6 */
}
#endif