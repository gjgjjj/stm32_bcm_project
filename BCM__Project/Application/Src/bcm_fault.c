#include "bcm_fault.h"
#include "osal.h"
#include "bsp_gpio.h"
#include "log_service.h"
#include "storage_manager.h"

#include "bcm_state.h"


// 直接使用故障事件类型存储状态，和底层枚举保持一致
static FaultEventType_t active_fault_table[4U];

static uint8_t red_blink_en = 0U;
static uint8_t beep_blink_en = 0U;

static osal_mutex_t fault_table_mtx;

//故障码映射数组下标
static uint8_t BcmFault_CodeIndex(FaultCode_t fault_code)
{
    uint8_t idx = 0U;
    switch(fault_code)
    {
        case FAULT_CODE_SENSOR:
            idx = 0U;
            break;
        case FAULT_CODE_CAN:
            idx = 1U;
            break;
        case FAULT_CODE_LIGHT:
            idx = 2U;
            break;
        case FAULT_CODE_LOCK:
            idx = 3U;
            break;
        default:
            Log_Print(LOG_ERROR,"BcmFault: 未知故障码:0x%02X\r\n",fault_code);
            break;
    }
    return idx;
}

void BcmFault_Init(void)
{
    osal_mutex_create(&fault_table_mtx);

    if(osal_mutex_take(&fault_table_mtx, 10U) == 0)
    {
        Log_Print(LOG_ERROR,"初始化故障表互斥锁失败\r\n");
        return;
    }
    for(uint8_t i = 0; i < 4U; i++)
    {
        active_fault_table[i] = FAULT_EVENT_CLEAR;
    }
    osal_mutex_give(&fault_table_mtx);

    StorageMgr_PrintAllFaultLog();
}


//接收故障事件，更新活跃故障表
void BcmFault_ReceiveFaultEvent(FaultCode_t fault_code, FaultEventType_t event)
{
    uint8_t idx = BcmFault_CodeIndex(fault_code);
    if(idx >= 4U)
    {
        Log_Print(LOG_ERROR,"BcmFault: 未知故障码:0x%02X\r\n",fault_code);
        return;
    }

    if(osal_mutex_take(&fault_table_mtx, 10U) == 0)
    {
        return;
    }
    active_fault_table[idx] = event;
    osal_mutex_give(&fault_table_mtx);
}


//计算故障等级，写入bcm_state
void BcmFault_SetFaultLevel(void)
{
    uint8_t fault_level = SYS_FAULT_NONE;
    uint8_t work_mode = 0U;  // 默认正常模式

    if(osal_mutex_take(&fault_table_mtx, 10U) == 0)
    {
        return;
    }

    //CAN故障：严重故障
    if(active_fault_table[1U] == FAULT_EVENT_ACTIVE)
    {
        fault_level = SYS_FAULT_SEVERE;
        work_mode = 1U;
    }
    else
    {
        //传感器、灯光、门锁故障：普通故障
        if( (active_fault_table[0U]==FAULT_EVENT_ACTIVE) ||
            (active_fault_table[2U]==FAULT_EVENT_ACTIVE) ||
            (active_fault_table[3U]==FAULT_EVENT_ACTIVE) )
        {
            fault_level = SYS_FAULT_NORMAL;
        }
    }

    osal_mutex_give(&fault_table_mtx);

    BCM_SetFaultLevel(fault_level);
    BCM_SetWorkMode(work_mode);
}


uint8_t BcmFault_GetHeartBeatMode(void)
{
    BCM_State state_snap;     //上一时刻快照
    BCM_GetState(&state_snap);
    return (state_snap.fault_level != SYS_FAULT_NONE) ? 1U : 0U;
}

//严重故障时：CAN指令不能控制执行器，本地按键不受限制
uint8_t BcmFault_SevereFaultLockActuator(void)
{
    BCM_State state_snap;
    BCM_GetState(&state_snap);
    if(state_snap.fault_level == SYS_FAULT_SEVERE)
    {
        return 1U;
    }
    return 0U;
}

//清除Flash历史故障记录 + 清空应用层活跃故障表
void BcmFault_ClearHistoryFault(void)
{
    if(osal_mutex_take(&fault_table_mtx, 10U) == 0)
    {
        return;
    }

    //清空当前活跃故障表：全部置为清除
    for(uint8_t i = 0; i < 4U; i++)
    {
        active_fault_table[i] = FAULT_EVENT_CLEAR;
    }

    osal_mutex_give(&fault_table_mtx);

    StorageMgr_ClearAllFault();
}


static void SetRedBlinkEn(uint8_t en)     // 设置红灯闪烁使能   1:闪烁
{
    red_blink_en = en;
    if(en == 0U)
    {
        FaultMgr_LedRedSet(0U);
    }
}

static void SetBeepBlinkEn(uint8_t en)     // 设置蜂鸣间歇鸣叫使能   1:间歇鸣叫
{
    beep_blink_en = en;
    if(en == 0U)
    {
        FaultMgr_BeepSet(0U);
    }
}

void AlarmTimerCallback(void *pvArg)
{
    (void)pvArg;
    if(red_blink_en)
    {
        FaultMgr_LedRedToggle();
    }
    if(beep_blink_en)
    {
        FaultMgr_BeepToggle();
    }
}


void BcmFault_LedBeep_Control(void)
{
    BCM_State state_snap;
    BCM_GetState(&state_snap);

    switch(state_snap.fault_level)
    {
        case SYS_FAULT_NONE:
            FaultMgr_LedGreenSet(1U);
            SetRedBlinkEn(0U);
            SetBeepBlinkEn(0U);
            break;
        case SYS_FAULT_NORMAL:
            FaultMgr_LedGreenSet(1U);
            SetRedBlinkEn(1U);
            SetBeepBlinkEn(0U);
            break;
        case SYS_FAULT_SEVERE:
            FaultMgr_LedGreenSet(0U);
            SetRedBlinkEn(1U);
            SetBeepBlinkEn(1U);
            break;
        default:
            FaultMgr_LedGreenSet(1U);
            SetRedBlinkEn(0U);
            SetBeepBlinkEn(0U);
            break;
    }
}
