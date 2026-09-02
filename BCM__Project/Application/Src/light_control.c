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
    const BCM_State *p_bcm = BCM_GetState();
    uint8_t new_mode = !p_bcm->light_work_mode;
    BCM_SetLightWorkMode(new_mode);
}

uint8_t LightControl_GetWorkMode(void)
{
    const BCM_State *p_bcm = BCM_GetState();
    return p_bcm->light_work_mode;
}


void LightControl_SetOutput(uint8_t state)
{
    if(state > 1U)
    {
        return;
    }
    const BCM_State *p_bcm = BCM_GetState();
    if(p_bcm->light_state == state)
    {
        return;
    }

    BCM_SetLightState(state);
}


void LightControl_Toggle(void)
{
    const BCM_State *p_bcm = BCM_GetState();       
    uint8_t new_state = !p_bcm->light_state;
    LightControl_SetOutput(new_state);         
}


void LightControl_Process(void)
{
    const BCM_State *p_bcm = BCM_GetState();
    uint8_t global_state = p_bcm->light_state;

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
