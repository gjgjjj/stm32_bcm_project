#include "can_protocol.h"
#include <string.h>
#include "log_service.h"

#define CAN_TX_ID_HEARTBEAT        0x020U
#define CAN_TX_ID_LIGHT_STATUS     0x031U
#define CAN_TX_ID_DOOR_STATUS      0x041U
#define CAN_TX_ID_FAULT_EVENT      0x003U

static uint8_t raw_fault_flag;        // 【硬件总线故障标记】0异常(BUSOFF/WARNING)，1正常

//缓存CAN接收队列句柄
static osal_queue_t can_rx_queue_handle = NULL;

static CAN_ErrorState_t can_err_state;


void CanProtocol_Init(void)
{
    raw_fault_flag = 1U;         //硬件默认正常
    can_err_state = CAN_ERR_OK;
    can_rx_queue_handle = NULL;
}


//刷新CAN硬件总线故障状态   周期调用
void CanProtocol_RefreshHwErrorState(void)
{
    can_err_state = BSP_CAN_GetErrorState();

    //硬件状态同步到对外raw_fault_flag
    if((can_err_state == CAN_ERR_BUSOFF) || (can_err_state == CAN_ERR_WARNING))
    {
        raw_fault_flag = 0U;
    }
    else
    {
        raw_fault_flag = 1U;
    }
}

// 获取CAN硬件总线综合故障标志
uint8_t CAN_Protocol_GetRawFault(void)
{
    
    return raw_fault_flag;
}


//CAN接收中断回调函数
static void CAN_Protocol_RxIsrCallback(const CAN_Message_t *rx_msg)
{
    if((can_rx_queue_handle == NULL) || (rx_msg == NULL))
    {
        return;
    }

    CAN_Message_t msg_cache = *rx_msg;
    msg_cache.timestamp = osal_get_ms();
    (void)osal_queue_send_from_isr(can_rx_queue_handle, &msg_cache);  //将报文拷进CAN接收队列
}


//传入CAN接收队列句柄，注册BSP接收回调
void CAN_Protocol_SetRxQueue(osal_queue_t rx_queue)
{
    can_rx_queue_handle = rx_queue;
    BSP_CAN_RegisterRxCallback(CAN_Protocol_RxIsrCallback);   // 将回调函数传给BSP层
}


//CAN报文接收解析入口           0:解析成功；1:DLC非法；2:ID不支持；3:信号值非法；4:空指针输入
uint8_t CAN_Protocol_ParseMessage(const CAN_Message_t *rx_msg, CAN_Command_t *p_cmd)
{
    if ((rx_msg == NULL) || (p_cmd == NULL))    //空指针保护
    {
        Log_Print(LOG_ERROR, "CAN报文解析错误：输入空指针");
        return 4U;
    }

    switch (rx_msg->id)                         //报文ID合法性过滤
    {
        case CAN_CMD_LIGHT:
        case CAN_CMD_LOCK:
        case CAN_CMD_FAULT_CLEAR:
            break;
        default:
            Log_Print(LOG_ERROR, "CAN报文解析错误：不支持的报文ID:0x%03X", rx_msg->id);
            return 2U;
    }

    if (rx_msg->dlc != 1U)                      //DLC长度校验
    {
        Log_Print(LOG_ERROR,"CAN报文解析错误：DLC非法，期望1，实际%d", rx_msg->dlc);
        return 1U;
    }

    uint8_t sig_value = rx_msg->data[0];        // 信号取值范围校验
    switch (rx_msg->id)
    {
        case CAN_CMD_LIGHT:
        case CAN_CMD_LOCK:
        case CAN_CMD_FAULT_CLEAR:
            if ((sig_value != 0x00U) && (sig_value != 0x01U))
            {
                Log_Print(LOG_ERROR, "CAN报文解析错误：信号值非法，value:0x%02X", sig_value);
                return 3U;
            }
            break;
    }

    p_cmd->msg_id = rx_msg->id;                //组装解析后的控制指令
    p_cmd->value = sig_value;

    switch (rx_msg->id)
    {
        case CAN_CMD_LIGHT:
            p_cmd->command = 0x01U;           // 灯光控制指令类型
            break;
        case CAN_CMD_LOCK:
            p_cmd->command = 0x02U;           // 门锁控制指令类型
            break;
        case CAN_CMD_FAULT_CLEAR:
            p_cmd->command = 0x03U;           // 故障清除指令类型
            break;
        default:
            break;
    }

    return 0U;                                // 全部校验通过，解析成功
}


 //组包设备心跳报文（ID:0x020）
uint8_t CAN_Protocol_Heartbeat(CAN_Message_t *tx_msg, uint8_t state)
{
    if (tx_msg == NULL)
    {
        return 0U;
    }

    memset(tx_msg, 0, sizeof(CAN_Message_t));
    tx_msg->id = CAN_TX_ID_HEARTBEAT;      
    tx_msg->dlc = 1U;          
    tx_msg->data[0] = state;

    return 1U;
}

//组包车灯状态报文（ID:0x031）
uint8_t CAN_Protocol_LightStatus(CAN_Message_t *tx_msg, uint8_t state)
{
    if (tx_msg == NULL)
    {
        return 0U;
    }

    memset(tx_msg, 0, sizeof(CAN_Message_t));
    tx_msg->id = CAN_TX_ID_LIGHT_STATUS;      
    tx_msg->dlc = 1U;
    tx_msg->data[0] = state;

    return 1U;
}

//组包门锁状态报文（ID:0x041）
uint8_t CAN_Protocol_DoorLockStatus(CAN_Message_t *tx_msg, uint8_t state)
{
    if (tx_msg == NULL)
    {
        return 0U;
    }

    memset(tx_msg, 0, sizeof(CAN_Message_t));
    tx_msg->id = CAN_TX_ID_DOOR_STATUS;      
    tx_msg->dlc = 1U;
    tx_msg->data[0] = state;

    return 1U;
}

//组包故障事件报文（ID:0x003）
uint8_t CAN_Protocol_FaultEvent(CAN_Message_t *tx_msg, uint8_t fault_id, uint8_t fault_st)
{
    if (tx_msg == NULL)
    {
        return 0U;
    }

    memset(tx_msg, 0, sizeof(CAN_Message_t));
    tx_msg->id = CAN_TX_ID_FAULT_EVENT;      
    tx_msg->dlc = 3U;          
    tx_msg->data[0] = fault_id;    // 故障码
    tx_msg->data[1] = fault_st;    // 故障状态
    return 1U;
}



