/**
 * @file task_modem_at.h
 * @brief USART2 owner, AT command executor and modem response parser.
 */

#ifndef TASK_MODEM_AT_H
#define TASK_MODEM_AT_H

#include "FreeRTOS.h"

BaseType_t task_modem_at_create(void);

#endif
