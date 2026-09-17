#include "system_manager.h"
#include "osal.h"
#include <string.h>

#include "log_service.h"
#include "executor_diag.h"
#include "signal_filter.h"
#include "can_protocol.h"
#include "fault_manager.h"
#include "key_manager.h"
#include "storage_manager.h"

#include "lock_control.h"
#include "bcm_state.h"
#include "light_control.h"
#include "welcome_light.h"
#include "bcm_fault.h"


// 队列句柄
static osal_queue_t can_rx_queue       = NULL;
static osal_queue_t fault_event_queue  = NULL;
static osal_queue_t key_cmd_queue      = NULL;
static osal_queue_t can_cmd_queue      = NULL;

// 任务句柄
static osal_task_handle_t can_proto_task;
static osal_task_handle_t fault_monitor_task;
static osal_task_handle_t input_collect_task;
static osal_task_handle_t bcm_control_task;
static osal_task_handle_t system_manage_task;
static osal_task_handle_t fault_process_task;

// 软件定时器‑心跳报文
static osal_timer_handle_t heartbeat_timer;
static osal_timer_handle_t alarm_timer;
static volatile uint8_t hb_tx_flag = 0U;

static void CanProtocolTask(void *pvParam);
static void FaultMonitorTask(void *pvParam);
static void InputCollectTask(void *pvParam);
static void BcmControlTask(void *pvParam);
static void SystemManageTask(void *pvParam);
static void FaultProcessTask(void *pvParam);


static void Sensor_FeedFaultMgr(void)
{
    uint8_t sensor_raw = SignalFilter_GetRawFault();
    FaultMgr_InputStatus(FAULT_CODE_SENSOR, sensor_raw);
}

static void Can_FeedFaultMgr(void)
{
    uint8_t can_raw = CAN_Protocol_GetRawFault();
    FaultMgr_InputStatus(FAULT_CODE_CAN, can_raw);
}

static void HeartBeatTimerCallback(void *pvArg)
{
    (void)pvArg;
    hb_tx_flag = 1U;
}


void SystemMgr_Init(void)
{
    Log_Service_Init();
    Log_SetOutputLevel(LOG_INFO);
    Log_SetDebugEnable(1U);
    Log_SetFaultEventPrintEnable(1U);

    can_rx_queue = osal_queue_create(12U, sizeof(CAN_Message_t));            // CAN 接收队列
    key_cmd_queue = osal_queue_create(8U, sizeof(Key_Event_t));              // 按键事件队列
    can_cmd_queue = osal_queue_create(8U, sizeof(CAN_Command_t));           // CAN 指令队列
    fault_event_queue = osal_queue_create(8U, sizeof(Fault_Event_t));       // 故障事件队列

    // if((can_rx_queue == NULL)||(key_event_queue==NULL)||(can_cmd_queue==NULL)||(fault_event_queue==NULL))
    // {
    //     Log_Print(LOG_ERROR,"SystemMgr：队列资源创建失败\r\n");
    //     SystemMgr_EnterSafeFaultMode();
    //     return;
    // }

    CanProtocol_Init();                          
    CAN_Protocol_SetRxQueue(can_rx_queue);      
    KeyMgr_Init(key_cmd_queue);      
    FaultMgr_Init(fault_event_queue);
    ExecutorDiag_Init();
    SignalFilter_Init();
    StorageMgr_Init();

    BCM_State_Init();
    LightControl_Init();
    LockControl_Init();
    WelcomeLight_Init();
    BcmFault_Init();

}


void SystemMgr_StartTasksAndTimer(void)
{
    //创建全部任务
    can_protocol_CreateTask();
    fault_monitor_CreateTask();
    input_collect_CreateTask();
    bcm_control_CreateTask();
    system_manage_CreateTask();
    fault_process_CreateTask();

    // 心跳定时器 2000ms
    uint8_t tmr_ret1 = osal_timer_create("hb_timer",2000U,1U,NULL,HeartBeatTimerCallback,&heartbeat_timer);
    if(tmr_ret1 == 1U)
    {
        osal_timer_start(heartbeat_timer);
    }
    else
    {
        Log_Print(LOG_ERROR, "SystemMgr: 心跳报文定时器创建失败\r\n");
    }
    
    // 声光报警定时器 500ms
    uint8_t tmr_ret2 = osal_timer_create("alarm_timer",500U,1U,NULL,AlarmTimerCallback,&alarm_timer);
    if(tmr_ret2 == 1U)
    {
        osal_timer_start(alarm_timer);
    }
    else
    {
        Log_Print(LOG_ERROR,"SystemMgr: 报警定时器创建失败\r\n");
    }
}

//===================== 各任务创建 =====================
void can_protocol_CreateTask(void)
{
    osal_task_create(CanProtocolTask, "can_proto_task", 512U, NULL, 5U, &can_proto_task);
    if(can_proto_task == NULL)
    {
        Log_Print(LOG_ERROR,"CAN协议处理任务创建失败\r\n");
    }
}

void fault_monitor_CreateTask(void)
{
    osal_task_create(FaultMonitorTask, "fault_monitor_task", 512U, NULL, 4U, &fault_monitor_task);
    if(fault_monitor_task == NULL)
    {
        Log_Print(LOG_ERROR,"故障监控任务创建失败\r\n");
    }
}

void input_collect_CreateTask(void)
{
    osal_task_create(InputCollectTask, "input_collect_task", 512U, NULL, 3U, &input_collect_task);
    if(input_collect_task == NULL)
    {
        Log_Print(LOG_ERROR,"输入采集任务创建失败\r\n");
    }
}

void bcm_control_CreateTask(void)
{
    osal_task_create(BcmControlTask, "bcm_control_task", 768U, NULL, 2U, &bcm_control_task);
    if(bcm_control_task == NULL)
    {
        Log_Print(LOG_ERROR,"BCM控制任务创建失败\r\n");
    }
}

void system_manage_CreateTask(void)
{
    osal_task_create(SystemManageTask, "system_manage_task", 512U, NULL, 1U, &system_manage_task);
    if(system_manage_task == NULL)
    {
        Log_Print(LOG_ERROR,"系统管理任务创建失败\r\n");
    }
}

void fault_process_CreateTask(void)
{
    osal_task_create(FaultProcessTask, "fault_process_task", 768U, NULL, 0U, &fault_process_task);
    if(fault_process_task == NULL)
    {
        Log_Print(LOG_ERROR,"故障处理任务创建失败\r\n");
    }
}



//-------------------------- 各任务函数 --------------------------
static void CanProtocolTask(void *pvParam)
{
    (void)pvParam;
    CAN_Message_t rx_msg;
    CAN_Command_t cmd;

    while(1)
    {
        //等待原始CAN报文
        if(osal_queue_recv(can_rx_queue, &rx_msg, OSAL_WAIT_FOREVER) == 1U)
        {
            uint8_t parse_ret = CAN_Protocol_ParseMessage(&rx_msg, &cmd);
            if(parse_ret == 0U)
            {
                uint8_t send_ret = osal_queue_send(can_cmd_queue, &cmd, 0U);
                if(send_ret != 1U)
                {
                    Log_Print(LOG_ERROR,"CanProtocolTask：CAN指令队列已满，丢弃\r\n");
                }
            }
            //解析失败直接丢弃，不处理
        }
    }
}


static void FaultMonitorTask(void *pvParam)
{
    (void)pvParam;
    osal_tick_t last_wake_tick = osal_get_tick();
    while(1)
    {
        LightControl_FeedFaultMgr();
        LockControl_FeedFaultMgr();
        Sensor_FeedFaultMgr();
        Can_FeedFaultMgr();
        
        osal_delay_ms_until(&last_wake_tick, 10U);
    }
}


//输入采集任务 
static void InputCollectTask(void *pvParam)
{
    (void)pvParam;
    osal_tick_t last_wake_tick = osal_get_tick();
    while(1)
    {
        SignalFilter_Scan();         // 信号滤波处理 + ADC光照采样

        KeyMgr_ScanProcess();        // 按键扫描，检测按键边沿，产生按键事件投递到队列

        osal_delay_ms_until(&last_wake_tick, 10U);
    }
}



static void BcmControlTask(void *pvParam)
{
    (void)pvParam;
    Key_Event_t         key_evt;
    CAN_Command_t       can_cmd;

    osal_tick_t last_wake_tick = osal_get_tick();

    while(1)
    {
        //处理按键事件队列
        if(osal_queue_recv(key_cmd_queue, &key_evt, 0U) == 1U)
        {
            switch(key_evt)
            {
                case KEY1_SHORT:
                    LightControl_Toggle();
                    break;
                case KEY1_LONG:
                    LightControl_ToggleWorkMode();
                    break;
                case KEY2_SHORT:
                    LockControl_Toggle();
                    break;
                case KEY2_LONG:
                    StorageMgr_ClearAllFault();
                    break;
                default:
                    break;
            }
        }

        //处理CAN控制指令队列
        if(osal_queue_recv(can_cmd_queue, &can_cmd, 0U) == 1U)
        {
            switch(can_cmd.msg_id)
            {
                case CAN_CMD_LIGHT:     
                {
                    uint8_t light_cmd = can_cmd.value;
                    LightControl_SetOutput(light_cmd);
                    break;
                }
                case CAN_CMD_LOCK:   
                {
                    uint8_t lock_cmd = can_cmd.value;
                    LockControl_SetOutput(lock_cmd);

                    break;
                }
                case CAN_CMD_FAULT_CLEAR:     
                {
                    uint8_t clear_flag = can_cmd.value;    
                    if(clear_flag == 0x01U)
                    {
                        //这个指令的作用我还需考虑
                    }
                    break;
                }
                default:
                    Log_Print(LOG_ERROR,"BcmControl: 未知CAN指令ID:0x%03X\r\n",can_cmd.msg_id);
                    break;
            }
        }

        LockControl_Process();
        LightControl_Process();

        ExecutorDiag_LightRoutine();
        ExecutorDiag_LockRoutine();

        if(LockControl_GetUnlock() == 1U)
        {
            WelcomeLight_OnUnlockEvent();
        }
        WelcomeLight_Process();

        osal_delay_ms_until(&last_wake_tick, 10U);
    }
}



static void SystemManageTask(void *pvParam)
{
    (void)pvParam;
    osal_tick_t last_wake_tick = osal_get_tick();

    while(1)
    {
        //BSP_IWDG_Feed();

        //更新全局故障等级和工作模式
        BcmFault_SetFaultLevel();

        //报警声光控制  
        BcmFault_LedBeep_Control(); 
        
        //发送心跳报文
        if(hb_tx_flag == 1U)
        {
            hb_tx_flag = 0U;
            CAN_Protocol_SendHeartbeat(BcmFault_GetHeartBeatMode());
        }

        osal_delay_ms_until(&last_wake_tick, 50U);
    }
}



static void FaultProcessTask(void *pvParam)
{
    (void)pvParam;
    Fault_Event_t fault_evt;

    while(1)
    {
        if(osal_queue_recv(fault_event_queue, &fault_evt, OSAL_WAIT_FOREVER) == 1U)
        {
            //将故障事件送入应用层故障模块，更新活跃故障表
            BcmFault_ReceiveFaultEvent(fault_evt.fault_code, fault_evt.event);

            //日志打印故障事件
            if(fault_evt.event == FAULT_EVENT_ACTIVE)
            {
                Log_PrintFaultEvent("故障激活, 故障码 = %02X\r\n", fault_evt.fault_code);
            }
            else if(fault_evt.event == FAULT_EVENT_CLEAR)
            {
                Log_PrintFaultEvent("故障消除, 故障码 = %02X\r\n", fault_evt.fault_code);
            }

            //组装Flash故障记录，写入环形日志区
            Fault_Record_t rec;
            rec.fault_code = fault_evt.fault_code;
            rec.status     = fault_evt.event;
            rec.timestamp  = osal_get_ms();
            rec.checksum = 0U;
            StorageMgr_WriteFaultLog(&rec);

            //CAN故障事件报文上报
            CAN_Protocol_SendFaultEvent(fault_evt.fault_code, fault_evt.event);

        }

    }

}

