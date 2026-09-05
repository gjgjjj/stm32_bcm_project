#include "lock_control.h"
#include "bcm_state.h"
#include "executor_diag.h"
#include "fault_manager.h"
#include "can_protocol.h"

static uint8_t last_lock_state;
static uint8_t unlock_flag;

static uint8_t lock_shadow;     // 同步影子

void LockControl_Init(void)
{
    lock_shadow = LOCK_OFF;
    unlock_flag = 0U;
    BCM_State state_snap;
    BCM_GetState(&state_snap);
    last_lock_state = state_snap.lock_state;
}

//CAN指令入口  设置车锁状态
void LockControl_SetOutput(uint8_t state)
{
    if(state > LOCK_ON)
    {
        return;
    }
    BCM_State state_snap;
    BCM_GetState(&state_snap);
    if(state_snap.lock_state == state)
    {
        return;
    }
    BCM_SetLockState(state);
}

//按键指令入口  切换车锁状态
void LockControl_Toggle(void)
{
    BCM_State state_snap;
    BCM_GetState(&state_snap);
    uint8_t new_state = !state_snap.lock_state;
    LockControl_SetOutput(new_state);
}

//车锁周期处理
void LockControl_Process(void)
{
    BCM_State state_snap;
    BCM_GetState(&state_snap);
    uint8_t curr_lock_state = state_snap.lock_state;

    // -------- 边沿检测逻辑 --------
    unlock_flag = 0U;
    //检测：上锁1 → 解锁0
    if(last_lock_state == LOCK_ON && curr_lock_state == LOCK_OFF)
    {
        unlock_flag = 1U;               //解锁事件触发
    }
    last_lock_state = curr_lock_state;

    // -------- 状态同步：单点下发底层、单点CAN上报 --------
    uint8_t global_state = state_snap.lock_state;
    if(global_state == lock_shadow)
    {
        return;
    }
    lock_shadow = global_state;
    ExecutorDiag_SetLockExpect(global_state);
    CAN_Protocol_SendLockStatus(global_state);
}

// 查询本周期是否发生解锁事件        1：发生
uint8_t LockControl_GetUnlock(void)
{
    return unlock_flag;
}


void LockControl_FeedFaultMgr(void)
{
    uint8_t lock_raw = ExecutorDiag_GetLockRawFault();
    FaultMgr_InputStatus(FAULT_CODE_LOCK, lock_raw);
}
