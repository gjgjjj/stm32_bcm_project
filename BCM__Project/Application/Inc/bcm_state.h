#ifndef __BCM_STATE_H
#define __BCM_STATE_H

#include <stdint.h>


//BCM全局状态结构体
typedef struct
{
    uint8_t         light_state;        // 灯光状态，0=关闭，1=开启
    uint8_t         lock_state;         // 门锁状态，0=解锁，1=上锁
    uint8_t         work_mode;          // 系统工作模式，0=正常模式，1=故障模式
    uint8_t         night_state;        // 昼夜环境状态：0=白天，1=夜晚
    uint8_t         fault_level;        // 故障等级：0=无故障
    uint8_t         light_work_mode;    // 灯光工作模式：0=手动模式，1=自动模式
} BCM_State;

//获取BCM全局状态指针,只能读
const BCM_State* BCM_GetState(void);

//初始化BCM全局状态
void BCM_State_Init(void);

// 更新灯光状态
void BCM_SetLightState(uint8_t state);

// 设置全局灯光工作模式
void BCM_SetLightWorkMode(uint8_t mode);

// 更新门锁状态
void BCM_SetLockState(uint8_t state);

// 更新系统工作模式
void BCM_SetWorkMode(uint8_t mode);

//更新昼夜状态
void BCM_SetNightState(uint8_t night);

// 设置故障等级
void BCM_SetFaultLevel(uint8_t level);


#endif
