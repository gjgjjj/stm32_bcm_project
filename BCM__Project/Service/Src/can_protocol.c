#include "can_protocol.h"
#include <string.h>
#include "log_service.h"

#define CAN_TX_ID_HEARTBEAT        0x100U
#define CAN_TX_ID_LIGHT_STATUS     0x122U
#define CAN_TX_ID_DOOR_STATUS      0x142U
#define CAN_TX_ID_FAULT_EVENT      0x61U

static uint8_t raw_fault_flag;        

//缓存CAN接收队列句柄
static osal_queue_t can_rx_queue_handle = NULL;

static CAN_ErrorState_t hw_state;

static osal_mutex_t can_tx_mutex;

typedef enum {
    CAN_STATE_OK = 0,           // 正常状态（主动错误）
    CAN_STATE_WARNING,          // 被动错误（仅告警）
    CAN_STATE_BUSOFF_RECOVERING,// Bus-Off 恢复中（计时+重试）
    CAN_STATE_BUSOFF_LOCKED     // Bus-Off 锁定（重试超限，等待复位）
} CanState_t;

static CanState_t can_state = CAN_STATE_OK;
static uint8_t busoff_retry_cnt = 0;
static uint32_t busoff_tick_start = 0;



void CanProtocol_Init(void)
{
    raw_fault_flag = 1U;         //硬件默认正常
    hw_state = CAN_ERR_OK;
    can_rx_queue_handle = NULL;
    osal_mutex_create(&can_tx_mutex);
}


void CanProtocol_RefreshHwErrorState(void)
{
    hw_state = BSP_CAN_GetErrorState();
    uint8_t current_raw_fault = 1U; 
    
    static uint32_t ok_stable_tick = 0; 

    switch (can_state)
    {
        case CAN_STATE_OK:
            if (hw_state == CAN_ERR_WARNING)
            {
                can_state = CAN_STATE_WARNING;
                Log_Print(LOG_INFO, "CAN进入被动错误(Warning)\r\n");
            }
            else if (hw_state == CAN_ERR_BUSOFF)
            {
                can_state = CAN_STATE_BUSOFF_RECOVERING;
                busoff_retry_cnt = 0U;
                busoff_tick_start = osal_get_ms();
                Log_Print(LOG_ERROR, "检测到CAN Bus-Off，启动恢复流程\r\n");
                current_raw_fault = 0U; 
            }
            else 
            {
                ok_stable_tick++;
                if(ok_stable_tick >= 200U)
                {
                    busoff_retry_cnt = 0U;    
                }
            }
            break;

        case CAN_STATE_WARNING:
            if (hw_state == CAN_ERR_OK)
            {
                can_state = CAN_STATE_OK;
                ok_stable_tick = 0;
                Log_Print(LOG_INFO, "CAN退出被动错误，恢复正常\r\n");
            }
            else if (hw_state == CAN_ERR_BUSOFF)
            {
                can_state = CAN_STATE_BUSOFF_RECOVERING;
                busoff_retry_cnt = 0U; 
                busoff_tick_start = osal_get_ms();
                Log_Print(LOG_ERROR, "Warning恶化为CAN Bus-Off，启动恢复流程\r\n");
                current_raw_fault = 0U; 
            }
            break;

        case CAN_STATE_BUSOFF_RECOVERING:
            current_raw_fault = 0U; 
            
            if ((osal_get_ms() - busoff_tick_start) >= 100U)
            {
                if (hw_state != CAN_ERR_BUSOFF)
                {
                    can_state = CAN_STATE_OK; 
                    ok_stable_tick = 0; 
                    Log_Print(LOG_INFO, "硬件退出Bus-Off，等待系统防抖确认\r\n");
                }
                else 
                {
                    if (busoff_retry_cnt < 3U)
                    {
                        busoff_retry_cnt++; 
                        BSP_CAN_Stop();
                        BSP_CAN_Start(); 
                        
                        busoff_tick_start = osal_get_ms(); 
                        Log_Print(LOG_ERROR, "Bus-Off重新初始化，当前重试次数: %d/3\r\n", busoff_retry_cnt);
                    }
                    else
                    {
                        can_state = CAN_STATE_BUSOFF_LOCKED;
                        Log_Print(LOG_ERROR, "Bus-Off重试3次失败，锁定故障状态\r\n");
                    }
                }
            }
            break;

        case CAN_STATE_BUSOFF_LOCKED:
            current_raw_fault = 0U; 
            break;
            
        default:
            can_state = CAN_STATE_OK;
            break;
    }
    
    raw_fault_flag = current_raw_fault;
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
    msg_cache.timestamp = osal_get_ms_from_isr();
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
        Log_Print(LOG_ERROR, "CAN报文解析错误：输入空指针\r\n");
        return 4U;
    }

    switch (rx_msg->id)                         //报文ID合法性过滤
    {
        case CAN_CMD_LIGHT:
        case CAN_CMD_LOCK:
        case CAN_CMD_FAULT_CLEAR:
            break;
        default:
            Log_Print(LOG_ERROR, "CAN报文解析错误：不支持的报文ID:0x%02X\r\n", rx_msg->id);
            return 2U;
    }

    if (rx_msg->dlc != 1U)                      //DLC长度校验
    {
        Log_Print(LOG_ERROR,"CAN报文解析错误：DLC非法，期望1，实际%d\r\n", rx_msg->dlc);
        return 1U;
    }

    uint8_t sig_value = rx_msg->data[0];        // 信号取值范围校验
    if ((sig_value != 0x00U) && (sig_value != 0x01U))
    {
        Log_Print(LOG_ERROR, "CAN报文解析错误：非法信号值:0x%02X\r\n", sig_value);
        return 3U;
    }

    p_cmd->msg_id = rx_msg->id;                //组装解析后的控制指令
    p_cmd->value = sig_value;

    return 0U;                                // 全部校验通过，解析成功
}


//组包设备心跳报文
static uint8_t CAN_Protocol_Heartbeat(CAN_Message_t *tx_msg, uint8_t state)
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

//组包车灯状态报文
static uint8_t CAN_Protocol_LightStatus(CAN_Message_t *tx_msg, uint8_t state)
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

//组包门锁状态报文
static uint8_t CAN_Protocol_DoorLockStatus(CAN_Message_t *tx_msg, uint8_t state)
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

//组包故障事件报文
static uint8_t CAN_Protocol_FaultEvent(CAN_Message_t *tx_msg, uint8_t fault_id, uint8_t fault_st)
{
    if (tx_msg == NULL)
    {
        return 0U;
    }

    memset(tx_msg, 0, sizeof(CAN_Message_t));
    tx_msg->id = CAN_TX_ID_FAULT_EVENT;      
    tx_msg->dlc = 2U;          
    tx_msg->data[0] = fault_id;    // 故障码
    tx_msg->data[1] = fault_st;    // 故障状态
    return 1U;
}


uint8_t CAN_Protocol_SendHeartbeat(uint8_t state)
{
    CAN_Message_t msg;
    if(CAN_Protocol_Heartbeat(&msg, state) != 1U)
    {
        return 0U;
    }

    //进入临界区，保护CAN硬件发送邮箱
    if(osal_mutex_take(&can_tx_mutex, 10U) == 0U)
    {
        Log_Print(LOG_ERROR,"CAN发送获取互斥锁超时\r\n");
        return 0U;
    }

    uint8_t ret = BSP_CAN_SendRawMessage(&msg);

    osal_mutex_give(&can_tx_mutex);
    return ret;
}

uint8_t CAN_Protocol_SendLightStatus(uint8_t state)
{
    CAN_Message_t msg;
    if(CAN_Protocol_LightStatus(&msg, state) != 1U)
    {
        return 0U;
    }

    if(osal_mutex_take(&can_tx_mutex, 10U) == 0U)
    {
        Log_Print(LOG_ERROR,"CAN发送获取互斥锁超时\r\n");
        return 0U;
    }

    uint8_t ret = BSP_CAN_SendRawMessage(&msg);
    osal_mutex_give(&can_tx_mutex);
    return ret;
}

uint8_t CAN_Protocol_SendLockStatus(uint8_t state)
{
    CAN_Message_t msg;
    if(CAN_Protocol_DoorLockStatus(&msg, state) != 1U)
    {
        return 0U;
    }

    if(osal_mutex_take(&can_tx_mutex, 10U) == 0U)
    {
        Log_Print(LOG_ERROR,"CAN发送获取互斥锁超时\r\n");
        return 0U;
    }

    uint8_t ret = BSP_CAN_SendRawMessage(&msg);
    osal_mutex_give(&can_tx_mutex);
    return ret;
}

uint8_t CAN_Protocol_SendFaultEvent(uint8_t fault_id, uint8_t fault_st)
{
    CAN_Message_t msg;
    if(CAN_Protocol_FaultEvent(&msg, fault_id, fault_st) != 1U)
    {
        return 0U;
    }

    if(osal_mutex_take(&can_tx_mutex, 10U) == 0U)
    {
        Log_Print(LOG_ERROR,"CAN发送获取互斥锁超时\r\n");
        return 0U;
    }
    
    uint8_t ret = BSP_CAN_SendRawMessage(&msg);
    osal_mutex_give(&can_tx_mutex);
    return ret;
}
