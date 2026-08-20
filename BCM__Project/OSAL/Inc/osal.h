#ifndef __OSAL_H
#define __OSAL_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"

#define OSAL_WAIT_FOREVER       0xFFFFFFFFU     //永久等待，用于锁、队列等待时间参数ms

typedef void* osal_queue_t;                     //OSAL抽象队列句柄

//互斥锁
typedef struct
{
    StaticSemaphore_t buf;  //FreeRTOS静态信号量内存缓冲区
    SemaphoreHandle_t hdl;  //信号量句柄
} osal_mutex_t;


void          osal_mutex_create(osal_mutex_t *m);                 //创建静态互斥锁
uint8_t       osal_mutex_take(osal_mutex_t *m, uint32_t ms);      //获取互斥锁
uint8_t       osal_mutex_give(osal_mutex_t *m);                   //释放互斥锁
void          osal_mutex_delete(osal_mutex_t *m);                 //删除互斥锁


osal_queue_t  osal_queue_create(uint32_t count, uint32_t item_size);            //创建消息队列
uint8_t       osal_queue_send(osal_queue_t q, const void *data, uint32_t ms);   //队列发送
uint8_t       osal_queue_recv(osal_queue_t q, void *data, uint32_t ms);         //队列接收
uint8_t       osal_queue_send_from_isr(osal_queue_t q, const void *data);       //中断上下文队列发送
void          osal_queue_delete(osal_queue_t q);                                //删除队列，释放动态资源


uint32_t     osal_get_ms(void);                 //获取系统时间戳，禁止中断调用
void         osal_msleep(uint32_t ms);          //延时ms毫秒，仅任务上下文，禁止中断调用
uint32_t     osal_get_ms_from_isr(void);        //中断上下文获取系统时间戳，禁止中断调用

void* osal_get_current_task(void);              //获取当前任务句柄
void osal_yield(void);                          //让出CPU，触发一次任务调度


#endif
