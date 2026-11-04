/* USER CODE BEGIN Header */
/*
 * Node 3 - A7680C outgoing voice call test
 *
 * USART2:
 *   PA2 TX -> module RXD
 *   PA3 RX <- module TXD
 *   115200 baud, 8N1
 *
 * Power on the modem manually.
 *
 * Automatically attempt one call after AT responds.
 * Send ATH after approximately 30 seconds.
 * Set g_callRequest = 1 to call again.
 */
/* USER CODE END Header */

#include "main.h"
#include "cmsis_os.h"

/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "A7680C.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
#define TEST_PHONE          "0399136893"
#define CALL_DURATION_MS    30000U
/* USER CODE END PD */

/* Keep handle for references in the existing interrupt file. */
CAN_HandleTypeDef hcan;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
TaskHandle_t xCallTaskHandle = NULL;

/*
 * Stage:
 * 0  = task not started
 * 1  = waiting for modem
 * 2  = checking AT
 * 3  = ready, waiting for request
 * 4  = sending dial command
 * 5  = monitoring call
 * 6  = hang-up command sent
 * 7  = modem reported call termination
 * 90 = UART error or modem ERROR response
 */
volatile uint32_t g_callStage = 0;

volatile uint32_t g_atOK = 0;
volatile uint32_t g_atFailCount = 0;

/* 1 at startup: automatically attempt one call. */
volatile uint32_t g_callRequest = 1;

/* Set to 1 in Watch to request early hang-up. */
volatile uint32_t g_hangupRequest = 0;

volatile uint32_t g_callAttempts = 0;
volatile uint32_t g_callElapsedMs = 0;
volatile uint32_t g_hangupCount = 0;

volatile uint32_t g_uartTxStatus = HAL_OK;
volatile uint32_t g_uartError = 0;

/*
 * Fatal errors:
 * 1 = oscillator
 * 2 = clock
 * 3 = UART1
 * 4 = UART2
 * 5 = task allocation
 * 6 = scheduler returned
 */
volatile uint32_t g_errorCode = 0;

/* Read as strings in Watch. */
char g_lastCommand[64];
char g_callRx[256];

static uint16_t callRxLength = 0;
/* USER CODE END PV */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);

/* USER CODE BEGIN PFP */
static void Task_CallTest(void *argument);
static bool Modem_Send(const char *command);
static void Modem_ResetResponse(void);
static void Modem_ReadByte(void);
static void Modem_DrainInput(void);
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

    /* Bounded drain; cannot wait forever. */
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
        /*
         * Retain the most recent bytes if the buffer fills.
         * This allows later termination responses to be seen.
         */
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

static void Task_CallTest(void *argument)
{
    char command[64];
    uint32_t start;
    uint32_t finalStage;
    bool terminated;

    (void)argument;

    /*
     * Only store the handles here.
     * Do not call A7680C_PowerOn(): modem powered manually.
     */
    A7680C_Init(&huart2,
                A7680C_PWRKEY_GPIO_Port,
                A7680C_PWRKEY_Pin);

    g_callStage = 1;
    vTaskDelay(pdMS_TO_TICKS(3000));

    for (;;)
    {
        /*
         * Check AT until the modem responds.
         * This does not consume g_callRequest.
         */
        if (g_atOK == 0U)
        {
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
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }
        }

        if (g_callRequest == 1U)
        {
            /* Consume exactly one call request. */
            g_callRequest = 0;
            g_hangupRequest = 0;
            g_callElapsedMs = 0;

            Modem_DrainInput();
            Modem_ResetResponse();

            snprintf(command,
                     sizeof(command),
                     "ATD%s;\r\n",
                     TEST_PHONE);

            g_callStage = 4;
            g_callAttempts++;

            if (!Modem_Send(command))
            {
                g_callStage = 90;
                g_atOK = 0;
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }

            /*
             * Monitor responses for up to 30 seconds.
             * OK only means command accepted, not answered.
             */
            g_callStage = 5;
            start = HAL_GetTick();
            terminated = false;
            finalStage = 6;

            while ((HAL_GetTick() - start) < CALL_DURATION_MS)
            {
                Modem_ReadByte();

                g_callElapsedMs = HAL_GetTick() - start;

                if (strstr(g_callRx, "ERROR") != NULL)
                {
                    finalStage = 90;
                    break;
                }

                if (strstr(g_callRx, "NO CARRIER") != NULL ||
                    strstr(g_callRx, "BUSY") != NULL ||
                    strstr(g_callRx, "NO ANSWER") != NULL ||
                    strstr(g_callRx, "NO DIALTONE") != NULL)
                {
                    terminated = true;
                    finalStage = 7;
                    break;
                }

                if (g_hangupRequest == 1U)
                {
                    break;
                }

                vTaskDelay(pdMS_TO_TICKS(1));
            }

            g_hangupRequest = 0;

            if (!terminated)
            {
                /*
                 * End the call at the time limit or on request.
                 * Preserve the dial response in g_callRx.
                 */
                if (Modem_Send("ATH\r\n"))
                {
                    g_hangupCount++;

                    start = HAL_GetTick();

                    while ((HAL_GetTick() - start) < 1000U)
                    {
                        Modem_ReadByte();
                        vTaskDelay(pdMS_TO_TICKS(1));
                    }
                }
                else
                {
                    finalStage = 90;
                }
            }

            g_callStage = finalStage;
        }

        /*
         * No automatic redial.
         * Set g_callRequest = 1 in Watch to try again.
         */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* USER CODE END 0 */

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();

    /* USER CODE BEGIN RTOS_THREADS */
    if (xTaskCreate(Task_CallTest,
                    "SIM_Call",
                    512,
                    NULL,
                    2,
                    &xCallTaskHandle) != pdPASS)
    {
        g_errorCode = 5;
        Error_Handler();
    }
    /* USER CODE END RTOS_THREADS */

    osKernelStart();

    g_errorCode = 6;
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

    /*
     * PWRKEY not driven by STM32 in this test.
     * Power on the modem using its board button.
     */
    GPIO_InitStruct.Pin = A7680C_PWRKEY_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    HAL_GPIO_Init(A7680C_PWRKEY_GPIO_Port, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */

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