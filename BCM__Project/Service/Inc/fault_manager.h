#ifndef __FAULT_MANAGER_H
#define __FAULT_MANAGER_H

#include <stdint.h>
#include "osal.h"

// 故障码定义
typedef enum
{
    FAULT_CODE_SENSOR    = 0x01U,    // 传感器异常
    FAULT_CODE_CAN       = 0x02U,    // CAN总线异常
    FAULT_CODE_LIGHT     = 0x03U,    // 灯光执行器故障
    FAULT_CODE_LOCK      = 0x04U     // 门锁执行器故障
    
} FaultCode_t;

// 故障事件类型
typedef enum
{
    FAULT_EVENT_ACTIVE  = 0x00U,     // 故障激活
    FAULT_EVENT_CLEAR   = 0x01U      // 故障消除
} FaultEventType_t;

// 对外故障事件结构体，投递至应用层队列
typedef struct
{
    FaultCode_t fault_code;        // 故障码
    FaultEventType_t event;        // 故障事件类型：激活 / 消除 
} Fault_Event_t;


//故障管理初始化    1成功，0队列空失败
uint8_t FaultMgr_Init(osal_queue_t fault_queue);

// 完成防抖判定，上报故障事件 （固定周期调用，禁止在中断中调用）    state 原始采样状态 0=异常，1=正常
void FaultMgr_InputStatus(FaultCode_t code, uint8_t state);

//查询指定故障当前激活状态   0故障存在；1故障消除
uint8_t  FaultMgr_IsFaultActive(FaultCode_t code);


// 对外声光控制接口
void FaultMgr_LedRedSet(uint8_t state);    //红灯控制 1亮，0灭
void FaultMgr_LedGreenSet(uint8_t state);  //绿灯控制 1亮，0灭
void FaultMgr_BeepSet(uint8_t state);       //蜂鸣器 1开，0关
void FaultMgr_LedRedToggle(void);
void FaultMgr_BeepToggle(void);
#endif

