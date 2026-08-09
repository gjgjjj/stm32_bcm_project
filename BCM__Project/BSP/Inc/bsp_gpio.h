#ifndef __BSP_GPIO_H
#define __BSP_GPIO_H

#include "stm32f1xx_hal.h"

typedef enum
{
    RELAY_LIGHT,
    RELAY_LOCK,
    FB_LOCK,
    FB_LIGHT,
    BEEP,
    RGB_RED,
    RGB_GREEN,
    KEY1,
    KEY2,
} GPIO_ID;

void BSP_GPIO_Init(void);
void GPIO_WritePin(GPIO_ID id, uint8_t state);  //state: 1有效, 0无效
uint8_t GPIO_ReadPin(GPIO_ID id);               //返回状态: 1有效, 0无效
void GPIO_TogglePin(GPIO_ID id);

#endif