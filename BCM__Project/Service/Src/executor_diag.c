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

    uint8_t fb_nc = GPIO_ReadPin(FB_LIGHT_NC);
    uint8_t fb_no = GPIO_ReadPin(FB_LIGHT_NO);

    if(light_settle_cnt > 0U)
    {
        // 继电器机械动作中，等待稳定
        light_settle_cnt--;
    }
    else
    {
        if(fb_nc == fb_no)
        {
            // NC与NO电平相同，故障
            light_raw_fault = 0U;
        }
        else
        {
            // NC、NO互斥，硬件链路正常，校验反馈是否符合指令
            uint8_t expect_nc, expect_no;
            if(light_cmd == 1U)  // 继电器吸合
            {
                expect_nc = 1U; // NC断开，上拉高
                expect_no = 0U; // NO接通GND，低
            }
            else 
            {
                expect_nc = 0U; 
                expect_no = 1U; 
            }
            // 校验两路反馈是否和预期一致
            if( (fb_nc == expect_nc) && (fb_no == expect_no) )
            {
                light_raw_fault = 1U;
            }
            else
            {
                light_raw_fault = 0U;
            }
        }
    }
}

void ExecutorDiag_LockRoutine(void)
{
    uint8_t lock_cmd = lock_expect;
    GPIO_WritePin(RELAY_LOCK, lock_cmd);

    uint8_t fb_nc = GPIO_ReadPin(FB_LOCK_NC);
    uint8_t fb_no = GPIO_ReadPin(FB_LOCK_NO);

    if(lock_settle_cnt > 0U)
    {
        // 继电器机械动作中，等待稳定
        lock_settle_cnt--;
    }
    else
    {
        if(fb_nc == fb_no)
        {
            // NC与NO电平相同，故障
            lock_raw_fault = 0U;
        }
        else
        {
            // NC、NO互斥，硬件链路正常，校验反馈是否符合指令
            uint8_t expect_nc, expect_no;
            if(lock_cmd == 1U)  // 继电器吸合
            {
                expect_nc = 1U; // NC断开，上拉高
                expect_no = 0U; // NO接通GND，低
            }
            else 
            {
                expect_nc = 0U; 
                expect_no = 1U; 
            }
            // 校验两路反馈是否和预期一致
            if( (fb_nc == expect_nc) && (fb_no == expect_no) )
            {
                lock_raw_fault = 1U;
            }
            else
            {
                lock_raw_fault = 0U;
            }
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
