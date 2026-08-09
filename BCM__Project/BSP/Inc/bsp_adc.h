#ifndef __BSP_ADC_H
#define __BSP_ADC_H

#include "stm32f1xx_hal.h"

void BSP_ADC_Init(void);
uint16_t ADC_GetValue(void);

#endif

