#include "fault_manager.h"
#include "log_service.h"
#include "bsp_gpio.h"
#include <string.h>

#define FAULT_ACTIVE_TICK           20U   // 连续20次上报异常 → 确认故障激活
#define FAULT_CLEAR_TICK            10U   // 连续10次上报正常 → 确认故障清除
#define FAULT_TOTAL_NUM             4U    // 故障码总数量


//单路故障状态
typedef struct
{
    uint8_t  curr_input;     // 原始输入状态：0=异常，1=正常
    uint8_t filter_cnt;     // 防抖计数
    uint8_t  fault_state;    // 故障状态：0=故障激活，1=故障消除
} FaultFilter_t;

static FaultFilter_t   fault_filter[FAULT_TOTAL_NUM] ;       // 每一路故障防抖状态数组
static osal_queue_t    fault_event_queue = NULL;             // 向外投递故障事件的队列句柄


//故障管理模块初始化
uint8_t FaultMgr_Init(osal_queue_t fault_queue)
{
    if (fault_queue == NULL)
    {
        Log_Print(LOG_ERROR, "初始化失败：队列句柄为空\r\n");
        return 0;
    }
    // 防重复初始化
    if(fault_event_queue != NULL)
    {
        Log_Print(LOG_ERROR,  "已完成初始化，无需重复执行\r\n");
        return 1;
    }

    fault_event_queue = fault_queue;             // 保存上层传入的队列句柄

    // 初始化所有故障防抖状态，上电默认全部正常
    for (uint8_t i = 0U; i < FAULT_TOTAL_NUM; i++)
    {
        fault_filter[i].curr_input   = 1U;
        fault_filter[i].filter_cnt   = 0U;
        fault_filter[i].fault_state = 1U;
    }

    return 1;
}


// 完成防抖判定，上报故障事件 （固定周期调用，禁止在中断中调用）    state 原始采样状态 0=异常，1=正常
void FaultMgr_InputStatus(FaultCode_t code, uint8_t state)
{
    uint8_t id = (uint8_t)code - 0x01U;

    if (id >= FAULT_TOTAL_NUM)
    {
        Log_Print(LOG_ERROR,"故障码无效:0x%02X\r\n", code);
        return; 
    }
    if (state > 1U)
    {
        Log_Print(LOG_ERROR,"状态参数非法: %d\r\n", state);
        return;
    }

    FaultFilter_t *p = &fault_filter[id];

    if (p->curr_input != state)            //检测到输入发生跳变直接清零防抖计数器
    {
        p->filter_cnt = 0U;
    }

    //==================== 原始输入 state == 0U，检测到异常 ====================
    if (state == 0U)
    {
        if (p->fault_state != 0U)   // 当前未激活故障，累积异常计数
        {
            p->filter_cnt++;

            if ((p->filter_cnt >= FAULT_ACTIVE_TICK))
            {
                p->fault_state = 0U;                  //故障激活
                p->filter_cnt = 0U;

                if (fault_event_queue != NULL)        // 投递故障激活事件
                {
                    Fault_Event_t event = {0};
                    event.fault_code = code;
                    event.event = FAULT_EVENT_ACTIVE;
                    uint8_t ret = osal_queue_send(fault_event_queue, &event, 0U);
                    if(ret != 1U)
                    {
                        Log_Print(LOG_ERROR,"故障事件队列发送失败，事件丢失\r\n");
                    }
                }
            }
        }
        else
        {
            p->filter_cnt = 0U;          //故障已激活，不计数
        }
    }
    //==================== 原始输入 state ==1U，检测到正常 ====================
    else
    {
        if (p->fault_state == 0U)                       //处于故障激活状态，执行恢复防抖
        {
            p->filter_cnt++;

            if (p->filter_cnt >= FAULT_CLEAR_TICK)      // 达到恢复阈值，执行故障清除跳变
            {
                p->fault_state = 1U;                    //故障消除
                p->filter_cnt = 0U;

                if (fault_event_queue != NULL)          //投递故障清除事件
                {
                    Fault_Event_t event = {0};
                    event.fault_code = code;
                    event.event = FAULT_EVENT_CLEAR;
                    uint8_t ret = osal_queue_send(fault_event_queue, &event, 0U);
                    if(ret != 1U)
                    {
                        Log_Print(LOG_ERROR,"故障事件队列发送失败，事件丢失\r\n");
                    }
                }
            }
        }
        else                                            // 本身就正常，不计数
        {
            p->filter_cnt = 0U;            
        }
    }
    p->curr_input = state;      // 全部逻辑处理完成后，更新保存上一次输入状态 
}

//查询指定故障当前激活状态
uint8_t FaultMgr_IsFaultActive(FaultCode_t code)
{
    uint8_t id = (uint8_t)code - 0x01U;
    if (id >= FAULT_TOTAL_NUM)
    {
        return 2U;   //非法故障码返回2
    }
    return fault_filter[id].fault_state;
}



void FaultMgr_LedRedSet(uint8_t state)
{
    GPIO_WritePin(RGB_RED, state);
}

void FaultMgr_LedGreenSet(uint8_t state)
{
    GPIO_WritePin(RGB_GREEN, state);
}

void FaultMgr_BeepSet(uint8_t state)
{
    GPIO_WritePin(BEEP, state);
}

void FaultMgr_LedRedToggle(void)
{
    GPIO_TogglePin(RGB_RED);
}

void FaultMgr_BeepToggle(void)
{
    GPIO_TogglePin(BEEP);
}