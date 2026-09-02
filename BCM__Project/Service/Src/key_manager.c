#include "key_manager.h"
#include "bsp_gpio.h"
#include "log_service.h"
#include <stdint.h>
#include <string.h>

typedef enum
{
    KEY_STATE_IDLE,        // 空闲状态：无按键按下
    KEY_STATE_PRESS,       // 检测到按下：启动防抖计时
    KEY_STATE_DEBOUNCE,    // 防抖计时：等待20ms判断是否稳定
    KEY_STATE_HOLD,       // 长按保持：防抖完成持续按下，计时判断是否达到1000ms长按阈值
    KEY_STATE_RELEASE     // 释放阶段：按键松开，生成对应短/长按事件后切回空闲
} KeyState_t;

// 单路按键状态
typedef struct
{
    uint8_t raw_state;       // 原始读取的按键状态，1按下，0松开
    uint8_t stable_state;    // 消抖之后稳定的按键状态，1按下，0松开
    uint32_t debounce_tick;  // 防抖起始毫秒时间戳，用于20ms消抖判断
    uint32_t hold_tick;      // 有效按下起始毫秒时间戳，用于1000ms长按判定
    KeyState_t cur_state;    // 当前按键状态
} KeyItem_t;

#define KEY_DEBOUNCE_MS    20U    // 消抖时间20ms
#define KEY_LONG_PRESS_MS 1000U  // 长按判定1000ms

// 缓存上层传入的按键事件队列句柄
static osal_queue_t key_event_queue = NULL;

// 两路按键状态数组
static KeyItem_t key_buf[KEY_ID_MAX];


uint8_t KeyMgr_Init(osal_queue_t evt_queue)
{
    // 参数校验
    if(evt_queue == NULL)
    {
        Log_Print(LOG_ERROR,"初始化失败：队列句柄为空\r\n");
        return 0;
    }
    // 防重复初始化
    if(key_event_queue != NULL)
    {
        Log_Print(LOG_INFO,"已完成初始化，无需重复执行\r\n");
        return 1;
    }
    
    key_event_queue = evt_queue;                        // 保存上层传入的队列句柄

    for(uint8_t i = 0; i < KEY_ID_MAX; i++)             // 初始化两路按键状态
    {
        memset(&key_buf[i], 0x00, sizeof(KeyItem_t));   //重置，清除旧数据
        key_buf[i].cur_state = KEY_STATE_IDLE;          //设置初始状态为空闲
    }
    Log_Print(LOG_DEBUG,"按键服务初始化成功\r\n");
    return 1;
}


void KeyMgr_ScanProcess(void)
{
    if(key_event_queue == NULL)   //未初始化直接退出
    {
        return;
    }
 
    // 全局统一获取当前毫秒戳
    uint32_t now_ms = osal_get_ms();

    // 遍历两路按键分别处理
    for(uint8_t key_id = 0; key_id < KEY_ID_MAX; key_id++)
    {
        KeyItem_t *p_key = &key_buf[key_id];
        Key_Event_t send_evt = 0;

        // 步骤1：读取按键状态
        if(key_id == KEY_ID_1)
        {
            p_key->raw_state = GPIO_ReadPin(KEY1);
        }
        else if(key_id == KEY_ID_2)
        {
            p_key->raw_state = GPIO_ReadPin(KEY2);
        }

        // 步骤2：五态状态机流转逻辑
        switch(p_key->cur_state)
        {
            // 状态1：空闲状态，等待按键按下
            case KEY_STATE_IDLE:
                if(p_key->raw_state == 1)
                {
                    p_key->cur_state = KEY_STATE_PRESS;
                }
                break;

            case KEY_STATE_PRESS:
                if(p_key->raw_state == 1)
                {
                    p_key->debounce_tick = now_ms;
                    p_key->cur_state = KEY_STATE_DEBOUNCE;
                }
                else
                {
                    p_key->cur_state = KEY_STATE_IDLE;
                }
                break;

            case KEY_STATE_DEBOUNCE:
                if(p_key->raw_state == 0)
                {
                    p_key->cur_state = KEY_STATE_IDLE;            // 抖动，退回空闲
                }
                else if((now_ms - p_key->debounce_tick) >= KEY_DEBOUNCE_MS)
                {
                    // 消抖完成，进入稳定按住
                    p_key->stable_state = 1;
                    p_key->hold_tick = now_ms;
                    p_key->cur_state = KEY_STATE_HOLD;
                }
                break;

            case KEY_STATE_HOLD:
                if(p_key->raw_state == 0)
                {
                    p_key->cur_state = KEY_STATE_RELEASE;
                }
                break;

            case KEY_STATE_RELEASE:
                // 松开瞬间发生抖动，又检测到按下，退回HOLD，不产生事件
                if(p_key->raw_state == 1)
                {
                    p_key->cur_state = KEY_STATE_HOLD;
                    break;
                }
                // 防抖清除稳定电平
                p_key->stable_state = 0;
                // 计算按住总时长
                uint32_t press_time = now_ms - p_key->hold_tick;

                // 填充对应事件
                if(key_id == KEY_ID_1)
                {
                    if(press_time >= KEY_LONG_PRESS_MS)
                        send_evt = KEY1_LONG;
                    else
                        send_evt = KEY1_SHORT;
                }
                else
                {
                    if(press_time >= KEY_LONG_PRESS_MS)
                        send_evt = KEY2_LONG;
                    else
                        send_evt = KEY2_SHORT;
                }

                // 非阻塞发送事件到队列
                uint8_t send_ret = osal_queue_send(key_event_queue, &send_evt, 0U);
                if(send_ret != 1 )   
                {
                    Log_Print(LOG_ERROR,"按键服务：事件队列已满，key_id=%d，按键事件丢失\r\n", key_id);   
                }

                // 事件生成完毕，切回空闲状态
                p_key->cur_state = KEY_STATE_IDLE;
                break;

            // 异常未知状态，强制复位防卡死
            default:
                p_key->cur_state = KEY_STATE_IDLE;
                Log_Print(LOG_ERROR,"按键服务：按键%d进入未知状态，强制复位\r\n", key_id);
                break;
        }
    }
}

uint8_t KeyMgr_GetFilter(uint8_t key_id)
{
    if(key_id >= KEY_ID_MAX)
    {
        Log_Print(LOG_ERROR,"按键服务：key_id=%d越界\r\n", key_id);
        return 0;
    }
    
    return key_buf[key_id].stable_state;
}

