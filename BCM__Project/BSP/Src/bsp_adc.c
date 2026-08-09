#include "bsp_adc.h"

extern ADC_HandleTypeDef hadc1;

void BSP_ADC_Init(void)
{
    HAL_ADC_Start(&hadc1);
}


//获取单次ADC原始采样值
uint16_t ADC_GetValue(void)
{
    HAL_ADC_Start(&hadc1);
    if(HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
    {
        return (uint16_t)HAL_ADC_GetValue(&hadc1);
    }
    return 0U;
}

