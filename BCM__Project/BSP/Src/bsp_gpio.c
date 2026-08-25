#include "bsp_gpio.h"

typedef struct
{
    GPIO_TypeDef *port;   
    uint16_t pin;         
    uint8_t active_level;  //实际有效电平
} GPIO_Map_t;

#define GPIO_ID_COUNT   (sizeof(gpio_map) / sizeof(GPIO_Map_t))   //元素总个数 = 数组总字节 ÷ 单个元素字节

static const GPIO_Map_t gpio_map[] =
{
    [RELAY_LIGHT]  = {GPIOC, GPIO_PIN_5,  1},
    [RELAY_LOCK]   = {GPIOC, GPIO_PIN_2,  1},
    [FB_LOCK]      = {GPIOC, GPIO_PIN_3,  1},
    [FB_LIGHT]     = {GPIOC, GPIO_PIN_4,  1},
    [BEEP]         = {GPIOC, GPIO_PIN_0,  1},
    [RGB_RED]      = {GPIOB, GPIO_PIN_5,  0},
    [RGB_GREEN]    = {GPIOB, GPIO_PIN_0,  0},
    [KEY1]         = {GPIOA, GPIO_PIN_0,  1},
    [KEY2]         = {GPIOC, GPIO_PIN_13, 1},
};

void BSP_GPIO_Init(void)
{
    /* 上电默认状态：全部关闭 */
    GPIO_WritePin(RELAY_LIGHT, 0);
    GPIO_WritePin(RELAY_LOCK,  0);
    GPIO_WritePin(BEEP,        0);
    GPIO_WritePin(RGB_RED,     0);
    GPIO_WritePin(RGB_GREEN,   0);
}

void GPIO_WritePin(GPIO_ID id, uint8_t state)
{
    if ((unsigned int)id >= GPIO_ID_COUNT)  //id越界保护
    {
        return;
    }

    const GPIO_Map_t *p = &gpio_map[id];
    uint8_t output_level;  //硬件输出电平

    if (state != 0)
    {
        output_level = p->active_level;
    }
    else
    {
        output_level = !p->active_level;
    }

    HAL_GPIO_WritePin(p->port, p->pin, (GPIO_PinState)output_level );
}

uint8_t GPIO_ReadPin(GPIO_ID id)
{
    if ((unsigned int)id >= GPIO_ID_COUNT)
    {
        return 0;
    }

    const GPIO_Map_t *p = &gpio_map[id];

    GPIO_PinState input_level = HAL_GPIO_ReadPin(p->port, p->pin);

    uint8_t logic_level = (input_level == GPIO_PIN_SET) ? 1 : 0;  //换算成1/0

    return (logic_level == p->active_level) ? 1 : 0;    //返回状态 
}

void GPIO_TogglePin(GPIO_ID id)
{
    if ((unsigned int)id >= GPIO_ID_COUNT)
        return;

    const GPIO_Map_t *p = &gpio_map[id];
    
    switch(id)
    {
        case RELAY_LIGHT:
        case RELAY_LOCK:
        case BEEP:
        case RGB_RED:
        case RGB_GREEN:
             HAL_GPIO_TogglePin(p->port, p->pin);
            break;
        case FB_LOCK:
        case FB_LIGHT:
        case KEY1:
        case KEY2:
        default:
            break;
    }
}


