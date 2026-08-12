#ifndef __BSP_UART_H
#define __BSP_UART_H

#include "stm32f1xx_hal.h"

void BSP_UART_Init(void);                   //串口外设初始化
void UART_SendString(const char *str);            //发送字符串
void UART_Printf(const char *format, ...);  //格式化打印输出

#endif










