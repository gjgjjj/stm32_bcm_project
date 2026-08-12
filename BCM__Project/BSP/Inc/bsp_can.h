#ifndef __BSP_CAN_H
#define __BSP_CAN_H

#include "stm32f1xx_hal.h"


typedef enum
{
    CAN_ERR_OK          = 0U,   // 正常
    CAN_ERR_WARNING     = 1U,   // 错误警告,发送 / 接收错误计数达到127
    CAN_ERR_BUSOFF      = 2U    // Bus‑Off总线关闭
} CAN_ErrorState_t;

// 统一CAN报文结构体
typedef struct
{
    uint32_t id;          // 11位标准帧ID
    uint8_t  dlc;         // 数据长度码
    uint8_t  data[8];     // 8字节数据载荷
    uint32_t timestamp;   // 接收时间戳(ms)
} CAN_Message_t;

void BSP_CAN_Prepare(void);
uint8_t BSP_CAN_SetFilter(uint8_t bank,uint32_t id,uint32_t mask,uint8_t fifo); //设置CAN过滤器
void BSP_CAN_Start(void);
uint8_t CAN_SendMessage(CAN_Message_t *msg);    //发送一帧标准CAN报文
uint8_t CAN_ReceiveMessage(CAN_Message_t *msg); //读取接收队列中的一帧CAN报文
CAN_ErrorState_t CAN_GetErrorState(void);       //获取CAN控制器当前错误状态

#endif

