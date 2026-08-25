#include "log_service.h"
#include "bsp_uart.h"
#include "osal.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#define LOG_TMP_BUF_SIZE    128U         //栈上临时格式化缓冲区大小

static osal_mutex_t log_mutex; 

static LogLevel_t log_output_level = LOG_INFO;
static uint8_t log_debug_en = 1U;
static uint8_t log_fault_event_en = 1U;

//日志服务模块初始化，上电调用一次
void Log_Service_Init(void)
{
    osal_mutex_create(&log_mutex);
}


void Log_SetOutputLevel(LogLevel_t lvl)
{
    log_output_level = lvl;
}

void Log_SetDebugEnable(uint8_t en)
{
    log_debug_en = (en != 0U) ? 1U : 0U;
}

void Log_SetFaultEventPrintEnable(uint8_t en)
{
    log_fault_event_en = (en != 0U) ? 1U : 0U;
}

// 内部通用发送函数，复用互斥锁和串口 
static void Log_CommonSend(const char *fmt, va_list ap)
{
    char tmp_buf[LOG_TMP_BUF_SIZE];
    int ret_len = vsnprintf(tmp_buf, LOG_TMP_BUF_SIZE, fmt, ap);

    uint16_t send_len;
    if(ret_len < 0)
    {
        send_len = 0U;
    }
    else if((size_t)ret_len >= LOG_TMP_BUF_SIZE)
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
}


//普通日志打印接口  level 日志等级 、 fmt 格式化字符串 、 ... 可变参数
void Log_Print(LogLevel_t level, const char *fmt, ...)
{
    if(log_debug_en == 0U)
    {
        return;
    }
    if(level < log_output_level)
    {
        return;
    }
    if(fmt == NULL)
    {
        return;
    }
    if(osal_mutex_take(&log_mutex, 10U) == 0)
    {
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    Log_CommonSend(fmt, ap);
    va_end(ap);

    osal_mutex_give(&log_mutex);
}


//故障事件打印接口
void Log_PrintFaultEvent(const char *fmt, ...)
{
    if(log_fault_event_en == 0U)
    {
        return;
    }
    if(fmt == NULL)
    {
        return;
    }
    if(osal_mutex_take(&log_mutex, 10U) == 0)
    {
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    Log_CommonSend(fmt, ap);
    va_end(ap);

    osal_mutex_give(&log_mutex);
}
