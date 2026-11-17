/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f1xx_it.h
  * @brief   This file contains the headers of the interrupt handlers.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STM32F1xx_IT_H
#define __STM32F1xx_IT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
/**
 * @brief Handle a non-maskable interrupt through the generated exception handler.
 */
void NMI_Handler(void);
/**
 * @brief Stop execution when the Cortex-M core reports a hard fault.
 */
void HardFault_Handler(void);
/**
 * @brief Stop execution when a memory management fault occurs.
 */
void MemManage_Handler(void);
/**
 * @brief Stop execution when a bus access fault occurs.
 */
void BusFault_Handler(void);
/**
 * @brief Stop execution when an instruction usage fault occurs.
 */
void UsageFault_Handler(void);
/**
 * @brief Provide the Cortex-M debug monitor exception entry point.
 */
void DebugMon_Handler(void);
/**
 * @brief Dispatch the CAN1 FIFO0 interrupt to HAL_CAN_IRQHandler.
 */
void USB_LP_CAN1_RX0_IRQHandler(void);
/**
 * @brief Dispatch the HAL time-base timer interrupt to HAL_TIM_IRQHandler.
 */
void TIM4_IRQHandler(void);
/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __STM32F1xx_IT_H */
