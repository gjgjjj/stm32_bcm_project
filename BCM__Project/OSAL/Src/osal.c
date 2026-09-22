#include "FreeRTOS.h"
#include "semphr.h"
#include "osal.h"
#include "task.h"
#include "timers.h"
#include "queue.h"



uint8_t osal_task_create(void (*task_func)(void *),
                        const char *name,
                        uint32_t stack_size,
                        void *arg,
                        uint8_t priority,
                        osal_task_handle_t *handle_out)
{
    TaskHandle_t xHandle = NULL;

    configSTACK_DEPTH_TYPE stack_depth = (configSTACK_DEPTH_TYPE)(stack_size / sizeof(StackType_t));

    BaseType_t ret = xTaskCreate(task_func,
                                 name,
                                 stack_depth,
                                 arg,
                                 priority,
                                 &xHandle);

    if(ret != pdPASS)
    {
        return 0U;
    }

    if(handle_out != NULL)
    {
        *handle_out = xHandle;
    }

    return 1U;
}

/********************************定时器接口*****************************/
typedef struct
{
    osal_timer_callback_t  user_cb;
    void                  *user_arg;
} OsalTimerCbWrapper_t;

// RTOS真正注册的壳回调
static void OsalTimer_RtosWrapper(TimerHandle_t xTimer)
{
    OsalTimerCbWrapper_t *p_wrap = pvTimerGetTimerID(xTimer);
    if((p_wrap != NULL) && (p_wrap->user_cb != NULL))
    {
        p_wrap->user_cb(p_wrap->user_arg);
    }
}

uint8_t osal_timer_create(const char * const pcTimerName,
                          uint32_t xTimerPeriodMs,
                          uint8_t uxAutoReload,
                          void * const pvTimerID,
                          osal_timer_callback_t pxCallbackFunction,
                          osal_timer_handle_t *handle_out)
{
    if(pcTimerName == NULL || pxCallbackFunction == NULL || handle_out == NULL)
    {
        return 0U;
    }

    // 分配包装结构体
    OsalTimerCbWrapper_t *p_wrap = pvPortMalloc(sizeof(OsalTimerCbWrapper_t));
    if(p_wrap == NULL)
    {
        return 0U;   //堆分配失败
    }
    p_wrap->user_cb  = pxCallbackFunction;
    p_wrap->user_arg = pvTimerID;

    TickType_t period_tick = (TickType_t)pdMS_TO_TICKS(xTimerPeriodMs);
    UBaseType_t rtos_autoreload = (uxAutoReload != 0U) ? pdTRUE : pdFALSE;

    TimerHandle_t tmr_handle = xTimerCreate(pcTimerName,
                                            period_tick,
                                            rtos_autoreload,
                                            p_wrap,      
                                            OsalTimer_RtosWrapper);

    if(tmr_handle == NULL)
    {
        vPortFree(p_wrap);   //创建失败，释放wrapper，防止内存泄漏
        return 0U;
    }

    *handle_out = tmr_handle;
    return 1U;   
}

uint8_t osal_timer_delete(osal_timer_handle_t handle)
{
    if(handle == NULL)
    {
        return 0U;
    }
    //先取出wrapper
    OsalTimerCbWrapper_t *p_wrap = pvTimerGetTimerID((TimerHandle_t)handle);

    BaseType_t ret = xTimerDelete((TimerHandle_t)handle, 0U);
    if(ret == pdPASS)
    {
        vPortFree(p_wrap);  //销毁成功释放wrapper内存
        return 1U;
    }
    return 0U;
}

//禁止中断中调用
uint8_t osal_timer_start(osal_timer_handle_t handle)
{
    if(handle == NULL)
    {
        return 0U;
    }
    BaseType_t ret = xTimerStart((TimerHandle_t)handle, 0U);
    return (ret == pdPASS) ? 1U : 0U;
}

//禁止中断中调用
uint8_t osal_timer_stop(osal_timer_handle_t handle)
{
    if(handle == NULL)
    {
        return 0U;
    }
    BaseType_t ret = xTimerStop((TimerHandle_t)handle, 0U);
    return (ret == pdPASS) ? 1U : 0U;
}


/********************************互斥锁接口*****************************/
void osal_mutex_create(osal_mutex_t *m)   //m 传入osal_mutex_t结构体指针，调用方提供内存
{
    if (m == NULL)
    {
        return;
    }
    // 将字节缓冲区强转为FreeRTOS静态信号量内存
    StaticSemaphore_t *p_sem_buf = (StaticSemaphore_t *)m->buf;
    // 静态创建互斥锁，内存由用户提供的buf承载，不占用系统堆
    m->hdl = xSemaphoreCreateMutexStatic(p_sem_buf);
}

uint8_t osal_mutex_take(osal_mutex_t *m, uint32_t ms)  // m 互斥锁指针 、ms 等待时间、1U 成功拿到锁
{
    if (m == NULL || m->hdl == NULL)
    {
        return 0U;
    }
    TickType_t wait_ticks;
    if (ms == OSAL_WAIT_FOREVER)
    {
        wait_ticks = portMAX_DELAY;         //永久等待锁
    }
    else
    {
        wait_ticks = pdMS_TO_TICKS(ms);     //毫秒转换成系统tick数
    }
    BaseType_t ret = xSemaphoreTake(m->hdl, wait_ticks);
    return (ret == pdPASS) ? 1U : 0U;
}

uint8_t osal_mutex_give(osal_mutex_t *m)   // m 互斥锁指针 、1U 释放成功
{
    if (m == NULL || m->hdl == NULL)
    {
        return 0U;
    }
    BaseType_t ret = xSemaphoreGive(m->hdl);
    return (ret == pdPASS) ? 1U : 0U;
}

void osal_mutex_delete(osal_mutex_t *m)    // m 互斥锁指针 、静态锁仅清空句柄
{
    if (m == NULL || m->hdl == NULL)
    {
        return;
    }
    //vSemaphoreDelete(m->hdl);  //静态创建，内存不在RTOS堆里，调用此函数会造成堆损坏
    m->hdl = NULL;
}
/******************************消息队列接口*************************************/

//count 队列深度、item_size 单个元素字节大小
osal_queue_t osal_queue_create(uint32_t count, uint32_t item_size)   
{
    if (count == 0U || item_size == 0U)
    {
        return NULL;
    }

    QueueHandle_t q = xQueueCreate((UBaseType_t)count, (UBaseType_t)item_size);
    return (osal_queue_t)q;
}

//q 队列句柄 、 data 待发送数据缓冲区指针 、 ms 等待超时时间
uint8_t osal_queue_send(osal_queue_t q, const void *data, uint32_t ms)
{
    if (q == NULL || data == NULL)
    {
        return 0U;
    }

    TickType_t wait_ticks;
    if (ms == OSAL_WAIT_FOREVER)
    {
        wait_ticks = portMAX_DELAY;
    }
    else
    {
        wait_ticks = pdMS_TO_TICKS(ms);
    }

    BaseType_t ret = xQueueSend((QueueHandle_t)q, data, wait_ticks);
    return (ret == pdPASS) ? 1U : 0U;
}

// q 队列句柄 、 data 接收数据的缓冲区 、 ms 等待超时ms
uint8_t osal_queue_recv(osal_queue_t q, void *data, uint32_t ms)
{
    if (q == NULL || data == NULL)
    {
        return 0U;
    }

    TickType_t wait_ticks;
    if (ms == OSAL_WAIT_FOREVER)
    {
        wait_ticks = portMAX_DELAY;
    }
    else
    {
        wait_ticks = pdMS_TO_TICKS(ms);
    }

    BaseType_t ret = xQueueReceive((QueueHandle_t)q, data, wait_ticks);
    return (ret == pdPASS) ? 1U : 0U;
}

//q 队列句柄 、 data 待发送报文
uint8_t osal_queue_send_from_isr(osal_queue_t q, const void *data)
{
    if (q == NULL || data == NULL)
    {
        return 0U;
    }

    BaseType_t xWoken = pdFALSE;  //标记，当入队操作唤醒了一个优先级比当前被中断任务更高的任务，FreeRTOS把该变量置为pdTRUE
    BaseType_t ret = xQueueSendFromISR((QueueHandle_t)q, data, &xWoken);  //中断版入队

    portYIELD_FROM_ISR(xWoken);    //若标记为真，中断退出后立刻切换运行高优先级任务。

    return (ret == pdPASS) ? 1U : 0U;
}

void osal_queue_delete(osal_queue_t q)
{
    if (q == NULL)
    {
        return;
    }
    vQueueDelete((QueueHandle_t)q);
}


/******************************时间与延时接口*************************************/
uint32_t osal_get_ms(void)
{
    TickType_t tick = xTaskGetTickCount();
    return (uint32_t)tick * (uint32_t)portTICK_PERIOD_MS;
}


osal_tick_t osal_get_tick(void)
{
    return (osal_tick_t)xTaskGetTickCount();
}

void osal_msleep(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));           //将毫秒转换成系统tick数，任务进入阻塞状态，CPU 交给别的任务
}

uint32_t osal_get_ms_from_isr(void)
{
    TickType_t tick = xTaskGetTickCountFromISR();
    return (uint32_t)tick * (uint32_t)portTICK_PERIOD_MS;
}


void osal_delay_ms_until(osal_tick_t *last_wake_tick, uint32_t cycle_ms)
{
    if(last_wake_tick == NULL || cycle_ms == 0U)
    {
        return;
    }
    TickType_t *p_real_tick = (TickType_t *)last_wake_tick;
    TickType_t cycle_ticks = pdMS_TO_TICKS(cycle_ms);
    vTaskDelayUntil(p_real_tick, cycle_ticks);
}


//获取当前任务句柄
void* osal_get_current_task(void)
{
    return xTaskGetCurrentTaskHandle();
}

//让出CPU，触发一次任务调度
void osal_yield(void)
{
    taskYIELD();
}



uint16_t osal_task_get_highwatermark(osal_task_handle_t h_task)
{
    return (uint16_t)uxTaskGetStackHighWaterMark(h_task);
}