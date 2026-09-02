#ifndef __LIGHT_CONTROL_H
#define __LIGHT_CONTROL_H

#include <stdint.h>

// 灯光状态 
#define LIGHT_OFF         (0U)    //关闭
#define LIGHT_ON          (1U)    //开启

// 灯光工作模式
#define LIGHT_MODE_MANUAL       (0U)    //手动模式
#define LIGHT_MODE_AUTO         (1U)    //自动模式

// 灯光模块初始化
void LightControl_Init(void);

// 切换灯光工作模式
void LightControl_ToggleWorkMode(void);

// 获取灯光当前工作模式
uint8_t LightControl_GetWorkMode(void);

// CAN指令入口  设置灯光状态
void LightControl_SetOutput(uint8_t state);

// 按键指令入口  切换灯光状态
void LightControl_Toggle(void);

//灯光周期处理
void LightControl_Process(void);

//投喂故障状态，发送故障事件队列
void LightControl_FeedFaultMgr(void);


#endif
