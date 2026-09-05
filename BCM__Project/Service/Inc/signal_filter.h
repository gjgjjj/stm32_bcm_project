#ifndef __SIGNAL_FILTER_H
#define __SIGNAL_FILTER_H

#include <stdint.h>

// 光照昼夜状态
typedef enum
{
    LIGHT_STATE_DAY,    // 白天
    LIGHT_STATE_NIGHT   // 黑夜
} Night_state_t;

// 传感器异常状态
typedef enum
{
    SENSOR_ERR,  // ADC采样超量程故障
    SENSOR_NORMAL,
} SensorStatus_t;

 //信号滤波模块初始化    1：初始化成功
uint8_t SignalFilter_Init(void);

 //滑动平均、滞回状态判定、异常检测   需固定周期调用（如10ms）
void SignalFilter_Scan(void);

//获取当前稳定昼夜状态
uint8_t SignalFilter_GetState(void);               // 0 白天、 1 黑夜

// 获取传感器故障状态
uint8_t SignalFilter_GetSensorErr(void);         // 0 异常、 1 正常

//获取本次采样瞬时原始故障标志  
uint8_t SignalFilter_GetRawFault(void);                 // 0 异常、 1 正常

#endif


