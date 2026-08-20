#include "bsp_adc.h"

extern ADC_HandleTypeDef hadc1;

void BSP_ADC_Init(void)
{
   
}


//获取单次ADC原始采样值
uint16_t BSP_ADC_GetValue(void)
{
    HAL_ADC_Start(&hadc1);
    uint16_t ret_val = 0U;

    /* 等待转换完成 */
    if(HAL_ADC_PollForConversion(&hadc1, 50U) == HAL_OK)
    {
        ret_val = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }

    return ret_val;
}


