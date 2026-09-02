#ifndef __LOCK_CONTROL_H
#define __LOCK_CONTROL_H

#include <stdint.h>

#define LOCK_OFF     (0U)   //解锁
#define LOCK_ON      (1U)   //上锁

void LockControl_Init(void);

//CAN指令入口  设置车锁状态
void LockControl_SetOutput(uint8_t state);

//按键指令入口  切换车锁状态
void LockControl_Toggle(void);

//投喂故障状态，发送故障事件队列
void LockControl_FeedFaultMgr(void);  

//车锁模块周期处理，10ms任务调用
void LockControl_Process(void);

//查询本周期是否发生解锁事件    1：本周期检测到解锁事件
uint8_t LockControl_GetUnlock(void);


#endif
