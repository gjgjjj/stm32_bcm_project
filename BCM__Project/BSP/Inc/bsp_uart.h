#ifndef __BSP_UART_H
#define __BSP_UART_H

#include "stm32f1xx_hal.h"

void BSP_UART_Init(void);
int BSP_UART_RawSend(const uint8_t *buf, uint16_t len, uint32_t timeout);
int BSP_UART_RawRecv(uint8_t *buf, uint16_t len, uint32_t timeout);
void BSP_UART_Stop(void);

#endif






