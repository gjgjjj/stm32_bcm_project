#include "executor_diag.h"
#include "bsp_gpio.h"

static uint8_t light_expect;      //灯光期望状态   0：关闭  1：打开
static uint8_t lock_expect;       //门锁期望状态   0：解锁  1：上锁
static uint8_t light_raw_fault;    //灯光原始故障状态  0：异常 1：正常
static uint8_t lock_raw_fault;    //门锁原始故障状态  0：异常 1：正常

//输出变化后，跳过n个周期再做故障对比
#define EXECUTOR_SETTLE_CNT  2U

static uint8_t light_settle_cnt;
static uint8_t lock_settle_cnt;

void ExecutorDiag_Init(void)
{
    light_expect = 0U;
    lock_expect = 0U;
    light_raw_fault = 1U;
    lock_raw_fault = 1U;

    light_settle_cnt = 0U;
    lock_settle_cnt = 0U;
}

void ExecutorDiag_SetLightExpect(uint8_t expect_state)
{
    if(light_expect != expect_state)
    {
        light_expect = expect_state;
        light_settle_cnt = EXECUTOR_SETTLE_CNT;
    }
}

void ExecutorDiag_SetLockExpect(uint8_t expect_state)
{
    if(lock_expect != expect_state)
    {
        lock_expect = expect_state;
        lock_settle_cnt = EXECUTOR_SETTLE_CNT;
    }
}

void ExecutorDiag_LightRoutine(void)
{
    uint8_t light_cmd = light_expect;
    GPIO_WritePin(RELAY_LIGHT, light_cmd);
    uint8_t real_fb = GPIO_ReadPin(FB_LIGHT);

    if(light_settle_cnt > 0U)
    {
        // 继电器机械动作中，等待稳定
        light_settle_cnt--;
    }
    else
    {
        if(light_cmd == real_fb)
        {
            light_raw_fault = 1U;
        }
        else
        {
            light_raw_fault = 0U;
        }
    }
}

void ExecutorDiag_LockRoutine(void)
{
    uint8_t lock_cmd = lock_expect;
    GPIO_WritePin(RELAY_LOCK, lock_cmd);
    uint8_t real_fb = GPIO_ReadPin(FB_LOCK);

    if(lock_settle_cnt > 0U)
    {
        // 继电器机械动作中，等待稳定
        lock_settle_cnt--;
    }
    else
    {
        if(lock_cmd == real_fb)
        {
            lock_raw_fault = 1U;
        }
        else
        {
            lock_raw_fault = 0U;
        }
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
