#include "bsp_can.h"

extern CAN_HandleTypeDef hcan;

static BSP_CAN_RxCallback_t can_rx_cb = NULL;   //回调指针  存上层回调函数的地址

static uint8_t started = 0U;     

uint8_t BSP_CAN_SetFilter(uint8_t bank,uint32_t id,uint32_t mask,uint8_t fifo)
{
    if(bank >= 14U || fifo > 1U)
    {
        return 0U;
    }

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
    if(started !=0U)
        return;

    HAL_CAN_Start(&hcan);
    __HAL_CAN_ENABLE_IT(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
    started = 1U;
}

void BSP_CAN_Stop(void)
{
    __HAL_CAN_DISABLE_IT(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
    HAL_CAN_Stop(&hcan);
    started = 0U;
}

// BSP底层原始发送接口  1:成功  0:失败
uint8_t BSP_CAN_SendRawMessage(const CAN_Message_t *msg)
{
    if(msg == NULL)
    {
        return 0U;
    }

    if(msg->dlc > 8U)
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


//获取CAN控制器错误状态     状态码 OK/WARNING/BUSOFF
CAN_ErrorState_t BSP_CAN_GetErrorState(void)
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


void BSP_CAN_RegisterRxCallback(BSP_CAN_RxCallback_t cb)
{
    can_rx_cb = cb;    // 将上层回调函数的地址存入指针
}


//CAN FIFO0接收中断回调   硬件读取、封装消息
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)  
{
    CAN_RxHeaderTypeDef rx_header;
    CAN_Message_t rx_msg = {0};

    if(HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_msg.data) == HAL_OK)
    {
        if(rx_header.DLC > 8U)
        {
            return;
        }
        rx_msg.id  = rx_header.StdId;
        rx_msg.dlc = rx_header.DLC;

        if(can_rx_cb != NULL)
        {
            can_rx_cb(&rx_msg);  //执行CAN协议模块内的回调函数，将报文拷入队列
        }
    }
}



