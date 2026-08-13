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


/*
#include "bsp_uart.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "semphr.h"

static SemaphoreHandle_t uart1_mutex;   //静态全局互斥锁，防止多任务同时打印乱码

extern UART_HandleTypeDef huart1;

void BSP_UART_Init(void)
{
    if(uart1_mutex == NULL)
    {
        uart1_mutex = xSemaphoreCreateMutex();
    }
}

//完成加锁 + 发送 + 解锁
static int UART_LockSend(const char *buf, uint16_t len)
{
    if(uart1_mutex == NULL)
    {
        return -1;
    }
    if(xSemaphoreTake(uart1_mutex, pdMS_TO_TICKS(1000U)) != pdTRUE)  // 获取互斥锁，最多等待1000ms
    {
        return -1;
    }

    HAL_StatusTypeDef st = HAL_UART_Transmit(&huart1, (uint8_t*)buf, len, 1000U);   //拿锁后，执行串口阻塞发送

    xSemaphoreGive(uart1_mutex);    // 释放互斥锁
    return (st == HAL_OK) ? 0 : -1;
}

void UART_SendString(const char *str)
{
    if(str == NULL) return;
    uint16_t len = (uint16_t)strlen(str);
    if(len == 0U) return;
    (void)UART_LockSend(str, len);
}

void UART_Printf(const char *format, ...)
{
    if(format == NULL) return;

    static char buf[128];    //格式化输出的缓冲区
    va_list ap;
    va_start(ap, format);
    int ret_len = vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);

    if(ret_len < 0) return;    //格式化失败，直接退出

    uint16_t send_len;
    if((size_t)ret_len >= sizeof(buf))
    {
        send_len = sizeof(buf)-1U;   //过长截断，留1字节给结束符\0
    }
    else
    {
        send_len = (uint16_t)ret_len;
    }

    (void)UART_LockSend(buf, send_len);
}
*/