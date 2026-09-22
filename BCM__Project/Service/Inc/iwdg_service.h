#ifndef IWDG_SERVICE_H
#define IWDG_SERVICE_H
#include "stdint.h"

typedef enum
{
    IWDG_TASK_FAULT_MONITOR,
    IWDG_TASK_INPUT_COLLECT,
    IWDG_TASK_BCM_CONTROL,
    IWDG_TASK_SYSTEM_MANAGE,
    IWDG_TASK_NUM  
}IwdgTaskId_t;

uint8_t IWDG_Service_Init(void);
void IWDG_Service_TaskRoutine(void);              // 看门狗服务周期函数
void IWDG_Service_TaskFeed(IwdgTaskId_t task_id); // 业务任务调用，上报存活
void BSP_SaveResetReason(void);
void BSP_PrintResetReason(void);

#endif
