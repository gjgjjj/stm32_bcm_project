#include "osal.h"
#include "task.h"

/********************************互斥锁接口*****************************/

void osal_mutex_create(osal_mutex_t *m)                  //m 传入osal_mutex_t结构体指针，调用方提供内存
{
    if (m == NULL)
    {
        return;
    }
    // 静态创建互斥锁，内存由用户提供的buf承载，不占用系统堆
    m->hdl = xSemaphoreCreateMutexStatic(&m->buf);
}

uint8_t osal_mutex_take(osal_mutex_t *m, uint32_t ms)   // m 互斥锁指针 、ms 等待时间、1U 成功拿到锁
{
    if (m == NULL || m->hdl == NULL)
    {
        return 0U;
    }

    TickType_t wait_ticks;
    if (ms == OSAL_WAIT_FOREVER)
    {
        wait_ticks = portMAX_DELAY;       //永久等待锁
    }
    else
    {
        wait_ticks = pdMS_TO_TICKS(ms);   //毫秒转换成系统tick数
    }

    BaseType_t ret = xSemaphoreTake(m->hdl, wait_ticks);
    return (ret == pdPASS) ? 1U : 0U;
}

uint8_t osal_mutex_give(osal_mutex_t *m)                 // m 互斥锁指针 、1U 释放成功
{
    if (m == NULL || m->hdl == NULL)
    {
        return 0U;
    }

    BaseType_t ret = xSemaphoreGive(m->hdl);
    return (ret == pdPASS) ? 1U : 0U;
}

void osal_mutex_delete(osal_mutex_t *m)                 // m 互斥锁指针 、静态锁仅清空句柄
{
    if (m == NULL || m->hdl == NULL)
    {
        return;
    }
    //vSemaphoreDelete(m->hdl);                       //静态创建，内存不在RTOS堆里，调用此函数会造成堆损坏
    m->hdl = NULL;
}


/******************************消息队列接口*************************************/

osal_queue_t osal_queue_create(uint32_t count, uint32_t item_size)   //count 队列深度、item_size 单个元素字节大小
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
uint32_t osal_get_ms(void)                   // 依赖 configTICK_RATE_HZ == 1000，1 tick = 1 ms
{
    return (uint32_t)xTaskGetTickCount();    //返回系统 tick 计数器，每 1ms 自增一次
}

void osal_msleep(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));           //将毫秒转换成系统tick数，任务进入阻塞状态，CPU 交给别的任务
}

uint32_t osal_get_ms_from_isr(void)
{
    return (uint32_t)xTaskGetTickCountFromISR();
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