#include "light_control.h"
#include "bcm_state.h"
#include "executor_diag.h"
#include "fault_manager.h"
#include "can_protocol.h"

static uint8_t light_shadow;    

void LightControl_Init(void)
{
    light_shadow = LIGHT_OFF;
}


void LightControl_ToggleWorkMode(void)
{
    BCM_State state_snap;
    BCM_GetState(&state_snap);
    uint8_t new_mode = !state_snap.light_work_mode;
    BCM_SetLightWorkMode(new_mode);
}

uint8_t LightControl_GetWorkMode(void)
{
    BCM_State state_snap;
    BCM_GetState(&state_snap);
    return state_snap.light_work_mode;
}


void LightControl_SetOutput(uint8_t state)
{
    if(state > 1U)
    {
        return;
    }
    BCM_State state_snap;
    BCM_GetState(&state_snap);
    if(state_snap.light_state == state)
    {
        return;
    }

    BCM_SetLightState(state);
}


void LightControl_Toggle(void)
{
    BCM_State state_snap;
    BCM_GetState(&state_snap);       
    uint8_t new_state = !state_snap.light_state;
    LightControl_SetOutput(new_state);         
}


void LightControl_Process(void)
{
    BCM_State state_snap;
    BCM_GetState(&state_snap);
    uint8_t global_state = state_snap.light_state;

    if(global_state == light_shadow)
    {
        return;
    }

    light_shadow = global_state;

    ExecutorDiag_SetLightExpect(global_state);
    CAN_Protocol_SendLightStatus(global_state);
}


void LightControl_FeedFaultMgr(void)
{
    uint8_t light_raw = ExecutorDiag_GetLightRawFault();
    FaultMgr_InputStatus(FAULT_CODE_LIGHT, light_raw);
}
