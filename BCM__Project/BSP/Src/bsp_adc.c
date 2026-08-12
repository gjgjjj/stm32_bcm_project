#include "bsp_adc.h"

extern ADC_HandleTypeDef hadc1;

void BSP_ADC_Init(void)
{
    HAL_ADC_Start(&hadc1);
}


//获取单次ADC原始采样值
uint16_t ADC_GetValue(void)
{
    uint16_t ret_val = 0U;

    HAL_ADC_Start(&hadc1);
    /* 等待转换完成 */
    if(HAL_ADC_PollForConversion(&hadc1, 10U) == HAL_OK)
    {
        ret_val = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);
    __HAL_ADC_CLEAR_FLAG(&hadc1, ADC_FLAG_EOC);  // 清除转换结束标志

    return ret_val;
}
