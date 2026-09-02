#include "bcm_state.h"

/* 全局BCM状态，本模块私有，外部只能通过 BCM_GetState() 只读获取 */
static BCM_State bcm_state = {
    .light_state  = 0U,
    .lock_state   = 0U,
    .work_mode    = 0U,
    .night_state  = 0U,
    .fault_level  = 0U,
    .light_work_mode = 0U,
};

const BCM_State* BCM_GetState(void)
{
    return &bcm_state;
}


void BCM_State_Init(void)
{
    bcm_state.light_state  = 0U;
    bcm_state.lock_state   = 1U;
    bcm_state.work_mode    = 0U;
    bcm_state.night_state  = 0U;
    bcm_state.fault_level  = 0U;
    bcm_state.light_work_mode = 1U;
}

void BCM_SetLightState(uint8_t state)
{
    if(state > 1U)
    {
        return;
    }
    bcm_state.light_state = state;
}

void BCM_SetLightWorkMode(uint8_t mode)
{
    bcm_state.light_work_mode = mode;
}

void BCM_SetLockState(uint8_t state)
{
    if(state > 1U)
    {
        return;
    }
    bcm_state.lock_state = state;
}

void BCM_SetWorkMode(uint8_t mode)
{
    if(mode > 1U)
    {
        return;
    }
    bcm_state.work_mode = mode;
}

void BCM_SetNightState(uint8_t night)
{
    if(night > 1U)
    {
        return;
    }
    bcm_state.night_state = night;
}

void BCM_SetFaultLevel(uint8_t level)
{
    bcm_state.fault_level = level;
}

