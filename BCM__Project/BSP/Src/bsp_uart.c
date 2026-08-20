#include "bsp_uart.h"

extern UART_HandleTypeDef huart1;

void BSP_UART_Init(void)
{
    
}

int BSP_UART_RawSend(const uint8_t *buf, uint16_t len, uint32_t timeout)
{
    if(buf == NULL || len == 0U)
    {
        return 0;    //入参非法
    }
    HAL_StatusTypeDef st = HAL_UART_Transmit(&huart1, buf, len, timeout);
    if(st == HAL_OK)
    {
        return 1;    //成功
    }
    return -1;       //超时
}

int BSP_UART_RawRecv(uint8_t *buf, uint16_t len, uint32_t timeout)
{
    if(buf == NULL || len == 0U)
    {
        return 0;   
    }
    HAL_StatusTypeDef st = HAL_UART_Receive(&huart1, buf, len, timeout);
    if(st == HAL_OK)
    {
        return 1;
    }
    return -1;
}

void BSP_UART_Stop(void)
{
    HAL_UART_DeInit(&huart1);
}


