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
    uint32_t timestamp;   // 时间戳
} CAN_Message_t;

typedef void (*BSP_CAN_RxCallback_t)(const CAN_Message_t *p_msg);   //定义回调函数格式：无返回、入参是只读CAN报文指针

void BSP_CAN_RegisterRxCallback(BSP_CAN_RxCallback_t cb);

uint8_t BSP_CAN_SetFilter(uint8_t bank,uint32_t id,uint32_t mask,uint8_t fifo); //设置CAN过滤器
void BSP_CAN_Start(void);
void BSP_CAN_Stop(void);

uint8_t BSP_CAN_SendRawMessage(const CAN_Message_t *msg);   //发送一帧标准CAN报文
CAN_ErrorState_t BSP_CAN_GetErrorState(void);     //获取CAN控制器当前错误状态

#endif


