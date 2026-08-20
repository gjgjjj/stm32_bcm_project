#ifndef __EXECUTOR_DIAG_H
#define __EXECUTOR_DIAG_H

#include <stdint.h>

//执行器诊断模块初始化
void ExecutorDiag_Init(void);

//设置车灯继电器期望输出指令
void ExecutorDiag_SetLightExpect(uint8_t expect_state);

//设置门锁继电器期望输出指令
void ExecutorDiag_SetLockExpect(uint8_t expect_state);

//车灯执行器周期诊断函数，10ms任务调用
void ExecutorDiag_LightRoutine(void);

//门锁执行器周期诊断函数，10ms任务调用
void ExecutorDiag_LockRoutine(void);

//获取车灯执行器原始故障状态  
uint8_t ExecutorDiag_GetLightRawFault(void);

//获取门锁执行器原始故障状态
uint8_t ExecutorDiag_GetLockRawFault(void);

#endif
