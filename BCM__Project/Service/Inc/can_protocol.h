#ifndef __CAN_PROTOCOL_H
#define __CAN_PROTOCOL_H

#include <stdint.h>
#include "osal.h"
#include "bsp_can.h"

#define CAN_CMD_LIGHT            0x21U    //灯光控制指令
#define CAN_CMD_LOCK             0x41U    //门锁控制指令
#define CAN_CMD_FAULT_CLEAR      0x62U    //故障清除指令

typedef struct
{
    uint32_t msg_id;     // 原始CAN报文ID
    uint8_t  value;      // 指令目标值
} CAN_Command_t;

void CanProtocol_Init(void);

//刷新CAN硬件总线故障状态   周期调用
void CanProtocol_RefreshHwErrorState(void);

// 获取CAN硬件总线综合故障标志
uint8_t CAN_Protocol_GetRawFault(void);

//设置CAN接收队列，注册底层BSP接收回调
void CAN_Protocol_SetRxQueue(osal_queue_t rx_queue);

// CAN报文接收解析入口
//rx_msg  输入：来自接收队列的原始CAN报文指针
//p_cmd 输出：解析成功后的控制指令
uint8_t CAN_Protocol_ParseMessage(const CAN_Message_t *rx_msg, CAN_Command_t *p_cmd);

//组包+发送，带互斥锁保护
uint8_t CAN_Protocol_SendHeartbeat(uint8_t state);
uint8_t CAN_Protocol_SendLightStatus(uint8_t state);
uint8_t CAN_Protocol_SendLockStatus(uint8_t state);
uint8_t CAN_Protocol_SendFaultEvent(uint8_t fault_id, uint8_t fault_st);


#endif
