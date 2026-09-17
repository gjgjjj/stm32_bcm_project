#ifndef __BCM_FAULT_H
#define __BCM_FAULT_H

#include <stdint.h>
#include "fault_manager.h"

//整机故障等级定义
#define SYS_FAULT_NONE        0U
#define SYS_FAULT_NORMAL      1U
#define SYS_FAULT_SEVERE      2U

void BcmFault_Init(void);

//接收故障事件，更新活跃故障表
void BcmFault_ReceiveFaultEvent(FaultCode_t fault_code, FaultEventType_t event);

//计算故障等级，更新全局状态
void BcmFault_SetFaultLevel(void);

//获取心跳报文模式（0正常 /1故障）
uint8_t BcmFault_GetHeartBeatMode(void);

//检查当前故障是否禁止CAN指令驱动执行器（严重故障返回1）
uint8_t BcmFault_SevereFaultLockActuator(void);

//清除历史故障记录
void BcmFault_ClearHistoryFault(void);

//故障声光控制
void BcmFault_LedBeep_Control(void);

void AlarmTimerCallback(void *pvArg);


#endif
