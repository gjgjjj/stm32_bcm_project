#include "executor_diag.h"
#include "bsp_gpio.h"

static uint8_t light_expect;
static uint8_t lock_expect;

static uint8_t light_raw_fault;
static uint8_t lock_raw_fault;


void ExecutorDiag_Init(void)
{
    light_expect = 0U;
    lock_expect = 0U;
    light_raw_fault = 1U;
    lock_raw_fault = 1U;
}

void ExecutorDiag_SetLightExpect(uint8_t expect_state)
{
    light_expect = expect_state;
}

void ExecutorDiag_SetLockExpect(uint8_t expect_state)
{
    lock_expect = expect_state;
}


void ExecutorDiag_LightRoutine(void)
{
    uint8_t real_fb;
    uint8_t light_cmd;

    light_cmd = light_expect;

    real_fb = GPIO_ReadPin(FB_LIGHT);  //读取的为有效状态

    if(light_cmd == real_fb)
    {
        light_raw_fault = 1U;
    }
    else
    {
        light_raw_fault = 0U;
    }
}


void ExecutorDiag_LockRoutine(void)
{
    uint8_t real_fb;
    uint8_t lock_cmd;

    lock_cmd = lock_expect;

    real_fb = GPIO_ReadPin(FB_LOCK);

    if(lock_cmd == real_fb)
    {
        lock_raw_fault = 1U;
    }
    else
    {
        lock_raw_fault = 0U;
    }
}


uint8_t ExecutorDiag_GetLightRawFault(void)
{
    return light_raw_fault;
}

uint8_t ExecutorDiag_GetLockRawFault(void)
{
    return lock_raw_fault;
}
