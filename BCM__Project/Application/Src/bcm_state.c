#include "bcm_state.h"
#include "osal.h"
#include <stddef.h>


// 全局BCM状态，本模块私有，外部只能通过 BCM_GetState() 只读获取
static BCM_State bcm_state = {
    .light_state  = 0U,
    .lock_state   = 0U,
    .work_mode    = 0U,
    .night_state  = 0U,
    .fault_level  = 0U,
    .light_work_mode = 0U,
};

//状态读写互斥锁
static osal_mutex_t bcm_state_mutex;

void BCM_State_Init(void)
{
    osal_mutex_create(&bcm_state_mutex);

    osal_mutex_take(&bcm_state_mutex, OSAL_WAIT_FOREVER);
    {
        bcm_state.light_state     = 0U;
        bcm_state.lock_state      = 0U;
        bcm_state.work_mode       = 0U;
        bcm_state.night_state     = 0U;
        bcm_state.fault_level     = 0U;
        bcm_state.light_work_mode = 1U;
    }
    osal_mutex_give(&bcm_state_mutex);
}

//获取BCM状态快照，拷贝到外部缓冲区   p_bcm 外部传入结构体指针，存放拷贝出来的状态
void BCM_GetState(BCM_State *p_bcm)
{
    if(p_bcm == NULL)
    {
        return;
    }
    osal_mutex_take(&bcm_state_mutex, OSAL_WAIT_FOREVER);
    {
        *p_bcm = bcm_state;
    }
    osal_mutex_give(&bcm_state_mutex);
}

void BCM_SetLightState(uint8_t state)
{
    if(state > 1U)
    {
        return;
    }
    osal_mutex_take(&bcm_state_mutex, OSAL_WAIT_FOREVER);
    {
        bcm_state.light_state = state;
    }
    osal_mutex_give(&bcm_state_mutex);
}

void BCM_SetLightWorkMode(uint8_t mode)
{
    osal_mutex_take(&bcm_state_mutex, OSAL_WAIT_FOREVER);
    {
        bcm_state.light_work_mode = mode;
    }
    osal_mutex_give(&bcm_state_mutex);
}

void BCM_SetLockState(uint8_t state)
{
    if(state > 1U)
    {
        return;
    }
    osal_mutex_take(&bcm_state_mutex, OSAL_WAIT_FOREVER);
    {
        bcm_state.lock_state = state;
    }
    osal_mutex_give(&bcm_state_mutex);
}

void BCM_SetWorkMode(uint8_t mode)
{
    if(mode > 1U)
    {
        return;
    }
    osal_mutex_take(&bcm_state_mutex, OSAL_WAIT_FOREVER);
    {
        bcm_state.work_mode = mode;
    }
    osal_mutex_give(&bcm_state_mutex);
}

void BCM_SetNightState(uint8_t night)
{
    if(night > 1U)
    {
        return;
    }
    osal_mutex_take(&bcm_state_mutex, OSAL_WAIT_FOREVER);
    {
        bcm_state.night_state = night;
    }
    osal_mutex_give(&bcm_state_mutex);
}

void BCM_SetFaultLevel(uint8_t level)
{
    osal_mutex_take(&bcm_state_mutex, OSAL_WAIT_FOREVER);
    {
        bcm_state.fault_level = level;
    }
    osal_mutex_give(&bcm_state_mutex);
}