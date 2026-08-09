#ifndef __BSP_IWDG_H
#define __BSP_IWDG_H

#include "stm32f1xx_hal.h"

void IWDG_Feed(void);  //看门狗喂狗，重置计数器

#endif
