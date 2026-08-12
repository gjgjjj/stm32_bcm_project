#include "bsp_can.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"


extern CAN_HandleTypeDef hcan;

QueueHandle_t can_rx_queue;


void BSP_CAN_Prepare(void)
{
    static uint8_t prepared = 0U;
    if(prepared != 0U)
        return;

    if(can_rx_queue == NULL)
    {
        can_rx_queue = xQueueCreate(8, sizeof(CAN_Message_t));
    }
    prepared = 1U;
}


uint8_t BSP_CAN_SetFilter(uint8_t bank,uint32_t id,uint32_t mask,uint8_t fifo)
{
    CAN_FilterTypeDef sFilterConfig = {0};
    sFilterConfig.FilterBank = bank;
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;

    sFilterConfig.FilterIdHigh = (id << 3) >> 16;
    sFilterConfig.FilterIdLow  = (id << 3) & 0xFFFFU;
    sFilterConfig.FilterMaskIdHigh = (mask <<3)>>16;
    sFilterConfig.FilterMaskIdLow  = (mask <<3)&0xFFFFU;

    sFilterConfig.FilterFIFOAssignment = fifo;
    sFilterConfig.FilterActivation = ENABLE;
    sFilterConfig.SlaveStartFilterBank = 14;

    if(HAL_CAN_ConfigFilter(&hcan,&sFilterConfig) != HAL_OK)
    {
        return 0U;
    }
    return 1U;
}

void BSP_CAN_Start(void)
{
    static uint8_t started = 0U;
    if(started !=0U)
        return;

    HAL_CAN_Start(&hcan);
    __HAL_CAN_ENABLE_IT(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
    started = 1U;
}

// 发送一帧标准CAN报文  1:成功  0:失败
uint8_t CAN_SendMessage(CAN_Message_t *msg)
{
    if(msg == NULL)
    {
        return 0U;
    }

    CAN_TxHeaderTypeDef tx_header;
    uint32_t tx_mailbox;

    tx_header.StdId  = msg->id;        //标准帧ID
    tx_header.IDE    = CAN_ID_STD;     //IDE=0  标准帧
    tx_header.RTR    = CAN_RTR_DATA;   //RTR=0  数据帧
    tx_header.DLC    = msg->dlc;       //数据长度

    if(HAL_CAN_AddTxMessage(&hcan, &tx_header, msg->data, &tx_mailbox) == HAL_OK)  //将报文打包放进发送邮箱
    {
        return 1U;
    }
    return 0U;
}


// 从接收队列读取一帧报文   1:读到报文 0:队列为空/失败
uint8_t CAN_ReceiveMessage(CAN_Message_t *msg)
{
    if(msg == NULL)
    {
        return 0U;
    }

    if(xQueueReceive(can_rx_queue, msg, 0U) == pdPASS)   //非阻塞读取，不等待，没有报文直接返回
    {
        return 1U;
    }
    return 0U;
}


//获取CAN控制器错误状态     状态码 OK/WARNING/BUSOFF
CAN_ErrorState_t CAN_GetErrorState(void)
{
    uint32_t err_flag = hcan.Instance->ESR;    //读取错误状态寄存器

    if((err_flag & CAN_ESR_BOFF) != 0U)       
    {
        return CAN_ERR_BUSOFF;
    }
    else if((err_flag & CAN_ESR_EPVF) != 0U)    //错误计数≥128
    {
        return CAN_ERR_WARNING;
    }
    return CAN_ERR_OK;
}



//CAN FIFO0接收中断回调   硬件读取、封装消息、入队
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)  
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;     //初始标记：没有唤醒高优先级任务
    CAN_RxHeaderTypeDef rx_header;
    CAN_Message_t rx_msg;

    if(can_rx_queue == NULL)
    {
        return;
    }

    if(HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_msg.data) == HAL_OK)
    {
        rx_msg.id        = rx_header.StdId;
        rx_msg.dlc       = rx_header.DLC;
        rx_msg.timestamp = xTaskGetTickCountFromISR();  //中断里获取系统节拍，作为接收时间戳，ms

        //把rx_msg整个拷贝进can_rx_queue队列
        //如果唤醒的任务任务优先级更高，修改 xHigherPriorityTaskWoken = pdTRUE
        xQueueSendFromISR(can_rx_queue, &rx_msg, &xHigherPriorityTaskWoken);  
    }
    //看标记，如果pdTRUE，中断退出瞬间触发任务切换
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

