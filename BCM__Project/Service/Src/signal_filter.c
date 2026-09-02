#include "signal_filter.h"
#include "bsp_adc.h"
#include <string.h>

#define FILTER_WIN_SIZE         8U
#define DAY_THR                 2800U
#define NIGHT_THR               1600U
#define ADC_MAX_VAL             4000U
#define ADC_ERR_CNT_MAX         5U          //连续N次异常，进入故障冻结
#define ADC_RECOVER_CNT         3U          //连续N次正常，才退出故障

static uint16_t filter_buf[FILTER_WIN_SIZE];  //环形缓存 ADC 原始采样
static uint8_t buf_idx;                       //环形缓存写索引
static uint32_t sum;                          //缓存区总和
static uint8_t buf_fill_cnt;                  //采集样本数

static Night_state_t light_state;              //光照状态
static SensorStatus_t sensor_status;          //传感器状态
static uint8_t err_cnt;                       //连续异常计数
static uint8_t recover_cnt;                   //连续正常恢复计数
static uint8_t raw_fault_flag;                //单次采样异常标记，仅输出给上层


uint8_t SignalFilter_Init(void)
{
    memset(filter_buf, 0, sizeof(filter_buf));
    buf_idx = 0U;
    sum = 0U;
    buf_fill_cnt = 0U;

    light_state = LIGHT_STATE_DAY;
    sensor_status = SENSOR_NORMAL;
    err_cnt = 0U;
    recover_cnt = 0U;
    raw_fault_flag = 1U;

    return 1U;
}


void SignalFilter_Scan(void)
{
    uint16_t raw_adc;
    uint16_t filter_avg;

    raw_adc = BSP_ADC_GetValue();

    /* ========== 瞬时原始故障判断 ========== */
    if( (raw_adc <= 10U) || (raw_adc >= ADC_MAX_VAL ) )
    {
        raw_fault_flag = 0U;
        err_cnt++;
        recover_cnt = 0U;       //出现异常，恢复计数器清零
    }
    else
    {
        raw_fault_flag = 1U;
        err_cnt = 0U;
        recover_cnt++;          //正常采样，恢复计数器累加
    }

    /* ========== 本地业务保护 ========== */
    if(sensor_status == SENSOR_ERR)
    {
        // 当前处于故障状态：需要连续多次正常才退出故障
        if(recover_cnt >= ADC_RECOVER_CNT)
        {
            sensor_status = SENSOR_NORMAL;
            recover_cnt = 0U;
        }
        else
        {
            // 仍然保持故障，冻结光照状态，不更新缓冲区
            return;
        }
    }
    else
    {
        // 当前正常状态，连续异常达到阈值进入故障
        if(err_cnt >= ADC_ERR_CNT_MAX)
        {
            sensor_status = SENSOR_ERR;
            return;
        }
    }

    // 采样状态正常，写入环形缓冲区，防止坏数据污染滤波
    sum = sum - filter_buf[buf_idx];
    filter_buf[buf_idx] = raw_adc;
    sum = sum + filter_buf[buf_idx];
    buf_idx++;
    if(buf_idx >= FILTER_WIN_SIZE)
    {
        buf_idx = 0U;
    }
    if(buf_fill_cnt < FILTER_WIN_SIZE)
    {
        buf_fill_cnt++;
    }
    filter_avg = (uint16_t)(sum / buf_fill_cnt);

    /* ========== 双阈值滞回光照判定 ========== */
    switch (light_state)
    {
        case LIGHT_STATE_DAY:
            if(filter_avg < NIGHT_THR)
            {
                light_state = LIGHT_STATE_NIGHT;
            }
            break;
        case LIGHT_STATE_NIGHT:
            if(filter_avg > DAY_THR)
            {
                light_state = LIGHT_STATE_DAY;
            }
            break;
    }
}

uint8_t SignalFilter_GetState(void)
{
    if(light_state == LIGHT_STATE_DAY)
    {
        return 0U;
    }
    else
    {
        return 1U;
    }
}

uint8_t SignalFilter_GetSensorErr(void)
{
    if(sensor_status == SENSOR_ERR)
    {
        return 0U;
    }
    else
    {
        return 1U;
    }
}


uint8_t SignalFilter_GetRawFault(void)    //传入故障管理模块 
{
    return raw_fault_flag;               
}
