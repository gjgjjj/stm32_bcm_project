#ifndef __KEY_MANAGER_H
#define __KEY_MANAGER_H

#include <stdint.h>
#include "osal.h"

// 按键ID枚举
typedef enum
{
    KEY_ID_1 = 0,
    KEY_ID_2,
    KEY_ID_MAX
}KEY_ID_t;

// 按键事件枚举   （入队列的）
typedef enum
{
    KEY1_SHORT = 0,
    KEY1_LONG,
    KEY2_SHORT,
    KEY2_LONG
}Key_Event_t;


// 按键管理模块初始化（上电执行一次清零）
uint8_t KeyMgr_Init(osal_queue_t evt_queue);  

//周期按键扫描处理函数，10ms周期
void KeyMgr_ScanProcess(void);

//获取指定按键防抖后稳定状态（由扫描函数更新的状态数据）
uint8_t KeyMgr_GetFilter(uint8_t key_id);

#endif


