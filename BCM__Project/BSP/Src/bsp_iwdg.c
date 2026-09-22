#include "bsp_iwdg.h"

#include "stm32f1xx_hal.h"

extern IWDG_HandleTypeDef hiwdg;

void BSP_IWDG_Feed(void)
{
    HAL_IWDG_Refresh(&hiwdg);
}