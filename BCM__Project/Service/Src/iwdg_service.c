#include "iwdg_service.h"
#include "stm32f1xx_hal.h"
#include "bsp_iwdg.h"
#include "osal.h"
#include "log_service.h"

static uint16_t task_alive_tick[IWDG_TASK_NUM] = {0};      // 每个任务的超时计数器数组

static uint8_t task_timeout_flag[IWDG_TASK_NUM] = {0};     // 超时打印标记

static osal_mutex_t iwdg_mutex;


//每个任务独立超时阈值    单位：系统管理任务周期(50ms)
static const uint16_t task_timeout_tick[IWDG_TASK_NUM] =
{
    20U,   // IWDG_TASK_FAULT_MONITOR     1s
    10U,    // IWDG_TASK_INPUT_COLLECT     500ms
    20U,   // IWDG_TASK_BCM_CONTROL       1s
    30U,   // IWDG_TASK_SYSTEM_MANAGE     1.5s
};


uint8_t IWDG_Service_Init(void)
{
    osal_mutex_create(&iwdg_mutex);

    for(uint8_t i = 0; i < IWDG_TASK_NUM; i++)
    {
        task_alive_tick[i] = 0U;
        task_timeout_flag[i] = 0U;
    }

    return 1U;
}


void IWDG_Service_TaskFeed(IwdgTaskId_t task_id)
{
    if(task_id < IWDG_TASK_NUM)
    {
        if(osal_mutex_take(&iwdg_mutex, 10U) == 0)
        {
            return;
        }
        task_alive_tick[task_id] = 0U;
        task_timeout_flag[task_id] = 0U;
        osal_mutex_give(&iwdg_mutex);
    }
}

void IWDG_Service_TaskRoutine(void)
{
    uint8_t all_task_ok = 1U;
    if(osal_mutex_take(&iwdg_mutex, 10U) == 0)
    {
        return;
    }

    for(uint8_t i = 0; i < IWDG_TASK_NUM; i++)
    {
        task_alive_tick[i]++;

        // 防止溢出
        if(task_alive_tick[i] > task_timeout_tick[i])
        {
            task_alive_tick[i] = task_timeout_tick[i];
        }

        //判断是否超时
        if(task_alive_tick[i] >= task_timeout_tick[i])
        {
            all_task_ok = 0U;
            if(task_timeout_flag[i] == 0U)
            {
                Log_Print(LOG_ERROR, "任务 %d 看门狗超时!\r\n", i);
                task_timeout_flag[i] = 1U;
            }
        }
    }
    osal_mutex_give(&iwdg_mutex);

    if(all_task_ok == 1U)
    {
        BSP_IWDG_Feed();
    }

}


static uint8_t reset_reason = 0U;
// 上电立刻获取上一次复位的原因
void BSP_SaveResetReason(void)
{
    if(__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST))
    {
        reset_reason = 1U;
    }
    else if(__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST))
    {
        reset_reason = 2U;
    }
    else if(__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST))
    {
        reset_reason = 3U;
    }
    else if(__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST))
    {
        reset_reason = 4U;
    }
    else
    {
        reset_reason = 0U;
    }
    __HAL_RCC_CLEAR_RESET_FLAGS();
}

// 打印上一次复位的原因
void BSP_PrintResetReason(void)
{
    switch(reset_reason)
    {
        case 1U:
            Log_Print(LOG_INFO, "IWDG独立看门狗复位\r\n");
            break;
        case 2U:
            Log_Print(LOG_INFO, "外部NRST引脚复位\r\n");
            break;
        case 3U:
            Log_Print(LOG_INFO, "NVIC软件复位\r\n");
            break;
        case 4U:
            Log_Print(LOG_INFO, "POR上电复位\r\n");
            break;
        default:
            Log_Print(LOG_INFO, "未知复位\r\n");
            break;
    }
}