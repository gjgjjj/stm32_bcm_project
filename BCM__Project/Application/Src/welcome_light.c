#include "welcome_light.h"
#include "bcm_state.h"
#include "signal_filter.h"
#include "osal.h"

static uint8_t  welcome_running;
static uint32_t welcome_start_ms;

void WelcomeLight_Init(void)
{
    welcome_running = 0U;
    welcome_start_ms = 0U;
}


static void WelcomeLight_RefreshNightState(void)
{
    uint8_t night_state = SignalFilter_GetState();
    BCM_SetNightState(night_state);
}


// 解锁事件入口
void WelcomeLight_OnUnlockEvent(void)
{
    WelcomeLight_RefreshNightState();
    
    const BCM_State *p_bcm = BCM_GetState();

    // 迎宾已经在运行，忽略本次事件
    if (welcome_running == 1U)
    {
        return;
    }

    // 自动模式才允许迎宾
    if (p_bcm->light_work_mode != 1U)
    {
        return;
    }

    // 夜晚才触发迎宾
    if (p_bcm->night_state != 1U)
    {
        return;
    }

    //灯光关闭
    if (p_bcm->light_state != 0U)
    {
        return;
    }

    // 全部条件满足，启动迎宾
    welcome_running = 1U;
    welcome_start_ms = osal_get_ms();
    BCM_SetLightState(1U);
}


//迎宾周期处理，BCM主任务10ms周期调用
void WelcomeLight_Process(void)
{
    if (welcome_running == 0U)
    {
        return;
    }

    const BCM_State *p_bcm = BCM_GetState();

    // 切为手动模式，终止迎宾
    if (p_bcm->light_work_mode == 0U)
    {
        welcome_running = 0U;
        return;
    }

    // 灯光被外部指令改写，退出迎宾
    if (p_bcm->light_state == 0U)
    {
        welcome_running = 0U;
        return;
    }

    // 迎宾计时到期，结束迎宾
    if (osal_get_ms() - welcome_start_ms >= WELCOME_LIGHT_DURATION_MS)
    {
        welcome_running = 0U;
        BCM_SetLightState(0U);
    }
}

uint8_t WelcomeLight_IsRunning(void)
{
    return welcome_running;
}
