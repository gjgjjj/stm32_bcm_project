#include "bsp_iwdg.h"

extern IWDG_HandleTypeDef hiwdg;

void BSP_IWDG_Feed(void)
{
    HAL_IWDG_Refresh(&hiwdg);
}