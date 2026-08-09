#include "bsp_uart.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "semphr.h"

extern SemaphoreHandle_t uart1_mutex;

extern UART_HandleTypeDef huart1;


void BSP_UART_Init(void)
{
#if 0
    uart1_mutex = xSemaphoreCreateMutex();
#endif
}

void UART_SendString(const char *str)
{
    if(str == NULL)
    {
        return;
    }
    uint16_t len = (uint16_t)strlen(str);
    if(len == 0U)
    {
        return;
    }

    /* RTOS互斥锁保护*/
#if 1
    if(xSemaphoreTake(uart1_mutex, pdMS_TO_TICKS(1000U)) != pdTRUE)
        return;
#endif

    HAL_StatusTypeDef status = HAL_UART_Transmit(&huart1, (uint8_t *)str, len, 1000U);
    (void)status;   

#if 1
    xSemaphoreGive(uart1_mutex);
#endif
}


void UART_Printf(const char *format, ...)
{
    if(format == NULL)
    {
        return;
    }

    char buf[128];
    va_list ap;
    va_start(ap, format);
    int ret_len = vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);

    if(ret_len < 0)
    {
        return;
    }

    size_t send_len;

    if((size_t)ret_len >= sizeof(buf))
    {
        send_len = sizeof(buf) - 1U;
    }
    else
    {
        send_len = (size_t)ret_len;
    }

#if 1
    if(xSemaphoreTake(uart1_mutex, pdMS_TO_TICKS(1000U)) != pdTRUE)
        return;
#endif

    HAL_StatusTypeDef status = HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)send_len, 1000U);
    (void)status;

#if 1
    xSemaphoreGive(uart1_mutex);
#endif
}
