#ifndef __SIGNAL_FILTER_H
#define __SIGNAL_FILTER_H

#include <stdint.h>
#include <stdbool.h>

// 光照昼夜状态
typedef enum
{
    LIGHT_STATE_DAY,    // 白天
    LIGHT_STATE_NIGHT   // 黑夜
} LightState_t;

// 传感器异常状态
typedef enum
{
    SENSOR_NORMAL,
    SENSOR_ERR,  // ADC采样超量程故障
} SensorStatus_t;

 //信号滤波模块初始化    1：初始化成功
uint8_t SignalFilter_Init(void);

 //滑动平均、滞回状态判定、异常检测   需固定周期调用（如10ms）
void SignalFilter_Scan(void);

//获取当前稳定昼夜状态
LightState_t SignalFilter_GetState(void);

// 获取传感器故障状态
SensorStatus_t SignalFilter_GetSensorErr(void);

//获取本次采样瞬时原始故障标志  0=异常，1=正常
uint8_t SignalFilter_GetRawFault(void);

#endif


