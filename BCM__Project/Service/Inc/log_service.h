#ifndef __LOG_SERVICE_H
#define __LOG_SERVICE_H

#include <stdint.h>

typedef enum
{
    LOG_DEBUG,    // 调试细节信息，开发阶段使用
    LOG_INFO,     // 正常运行状态信息
    LOG_ERROR     // 错误与异常信息
}LogLevel_t;

// 日志等级控制
void Log_SetOutputLevel(LogLevel_t lvl);
// 普通日志开关
void Log_SetDebugEnable(uint8_t en);
// 故障事件打印开关
void Log_SetFaultEventPrintEnable(uint8_t en);

// 普通日志打印接口
void Log_Print(LogLevel_t level, const char *fmt, ...);
// 故障事件打印接口
void Log_PrintFaultEvent(const char *fmt, ...);

#endif

