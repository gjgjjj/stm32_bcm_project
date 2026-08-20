#include "log_service.h"
#include "bsp_uart.h"
#include "osal.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#define LOG_TMP_BUF_SIZE    128U         //栈上临时格式化缓冲区大小

static osal_mutex_t log_mutex; 

//日志服务模块初始化，上电调用一次
void Log_Service_Init(void)
{
    (void)osal_mutex_create(&log_mutex);
}

//日志打印接口  level 日志等级 、 fmt 格式化字符串 、 ... 可变参数
void Log_Print(LogLevel_t level, const char *fmt, ...)
{
    if (level < LOG_OUTPUT_LEVEL)
    {
        return;
    }

    if(fmt == NULL)
    {
        return;
    }

    if (osal_mutex_take(&log_mutex, 0U) != 0)
    {
        return;
    }

    char tmp_buf[LOG_TMP_BUF_SIZE];
    va_list ap;
    va_start(ap, fmt);

    int ret_len = vsnprintf(tmp_buf, LOG_TMP_BUF_SIZE, fmt, ap);
    va_end(ap);

    uint16_t send_len;
    if(ret_len < 0)                                         //格式化错误
    {
        send_len = 0U;
    }
    else if((size_t)ret_len >= LOG_TMP_BUF_SIZE)            //内容超出缓冲区，发生截断
    {
        send_len = (uint16_t)(LOG_TMP_BUF_SIZE - 1U);      
    }
    else
    {
        send_len = (uint16_t)ret_len;
    }

    if(send_len > 0U)
    {
        (void)BSP_UART_RawSend((const uint8_t *)tmp_buf, send_len, 500U);
    }

    osal_mutex_give(&log_mutex);
}
