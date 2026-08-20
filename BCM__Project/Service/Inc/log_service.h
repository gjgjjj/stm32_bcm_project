#ifndef __LOG_SERVICE_H
#define __LOG_SERVICE_H

#include <stdint.h>

typedef enum
{
    LOG_DEBUG,    // 调试细节信息，开发阶段使用
    LOG_INFO,     // 正常运行状态信息
    LOG_ERROR     // 错误与异常信息
}LogLevel_t;

//全局日志输出等级控制宏：只输出 >=该等级的日志
#define LOG_OUTPUT_LEVEL    LOG_DEBUG

//日志打印接口
void Log_Print(LogLevel_t level,const char *fmt, ...);

#endif

