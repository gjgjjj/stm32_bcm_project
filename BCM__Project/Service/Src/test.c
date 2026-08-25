#include "test.h"
#include "osal.h"
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include "log_service.h"

#include "signal_filter.h"

#include "fault_manager.h"

#include "can_protocol.h"

#include "key_manager.h"

#include "storage_manager.h"

#include "executor_diag.h"

#if ENABLE_TEST_CODE

//第二个任务：并发打印，测试日志互斥锁 
static void Log_ConcurrentTask(void *arg)
{
    (void)arg;
    uint32_t cnt = 0U;
    for(;;)
    {
        Log_Print(LOG_INFO,"【任务B】计数 = %lu\r\n", cnt++);
        osal_msleep(250);               //任务B阻塞250ms，释放CPU
    }
}

static void Osal_Log_TestTask(void *arg)
{
    (void)arg;

    /* ========== OSAL基础能力测试 ========== */
    osal_mutex_t mtx;
    osal_mutex_create(&mtx);

    uint8_t ret_take = osal_mutex_take(&mtx, 100U);
    uint8_t ret_give = osal_mutex_give(&mtx);

    osal_queue_t q = osal_queue_create(4, sizeof(uint32_t));
    uint32_t send_val = 1234U;
    uint32_t recv_val = 0U;

    osal_queue_send(q, &send_val, 100U);
    osal_queue_recv(q, &recv_val, 100U);


    osal_task_create(Log_ConcurrentTask, "log_conc", 512, NULL, 2, NULL);    //把任务 B 加入就绪队列

    /* ========== 日志功能测试 ========== */
    Log_Print(LOG_INFO,"==== OSAL与日志模块测试 ====\r\n");
    Log_Print(LOG_INFO,"互斥锁 获取:%d 释放:%d\r\n", ret_take, ret_give);
    Log_Print(LOG_INFO,"队列 发送:%lu 接收:%lu\r\n", send_val, recv_val);
    Log_Print(LOG_INFO,"当前系统节拍 = %lu 毫秒\r\n", osal_get_ms());

    Log_Print(LOG_DEBUG,"这是调试级别信息\r\n");  //调试级别信息，不会打印
    Log_Print(LOG_ERROR,"错误测试示例\r\n");

    Log_Print(LOG_INFO, NULL);    //未死机则通过

    //测试超长字符串截断
    char long_buf[150];
    memset(long_buf, 'X', sizeof(long_buf));
    long_buf[149] = '\0';    //把最后一格改写为结束符
    Log_Print(LOG_INFO,"超长字符串测试: %s\r\n", long_buf);


    //主任务循环打印，与Log_ConcurrentTask并发输出
    uint32_t loop_cnt = 0U;
    for(;;)
    {
        Log_Print(LOG_INFO,"【任务A】循环计数 = %lu\r\n", loop_cnt++);
        uint32_t tick = osal_get_ms();
        Log_Print(LOG_INFO,"系统时间(ms): %lu\r\n", tick);
        osal_msleep(500);               //任务A阻塞500ms，释放CPU
    }
}

void Test_Osal_CreateTask(void)
{
    osal_task_create(Osal_Log_TestTask, "osal_log_test", 768U, NULL, 2U, NULL);
}


static void Test_SignalFilter_Task(void *arg)
{
    (void)arg;
    LightState_t light_st;
    SensorStatus_t sensor_st;
    uint8_t raw_fault;

    uint8_t ret = SignalFilter_Init();
    Log_Print(LOG_INFO,"SignalFilter_Init ret:%d\r\n", ret);

    uint32_t scan_tick = osal_get_ms();    //10ms扫描计时
    uint32_t log_tick  = osal_get_ms();    //2s打印计时

    while(1)
    {
        // 10ms执行一次 SignalFilter_Scan
        if((osal_get_ms() - scan_tick) >= 10U)
        {
            scan_tick = osal_get_ms();
            SignalFilter_Scan();
        }

        // 每2000ms打印一次状态
        if((osal_get_ms() - log_tick) >= 2000U)
        {
            log_tick = osal_get_ms();
            light_st = SignalFilter_GetState();
            sensor_st = SignalFilter_GetSensorErr();
            raw_fault = SignalFilter_GetRawFault();

            Log_Print(LOG_INFO,"light:%d  sensor_err:%d  raw_fault:%d\r\n",light_st, sensor_st, raw_fault);
        }

        osal_msleep(1U);   //释放CPU，降低占用，1ms轮询
    }
}

void Test_SignalFilter_CreateTask(void)
{
    osal_task_create(Test_SignalFilter_Task, "sig_filter_test", 1024U, NULL, 2U, NULL);
}



static void Test_FaultMgr_Task(void *arg)
{
    (void)arg;
    osal_queue_t fault_test_queue;
    uint8_t ret;
    Fault_Event_t evt_buf;

    //创建故障事件队列，用于接收FaultMgr投递出来的事件
    fault_test_queue = osal_queue_create(8U, sizeof(Fault_Event_t));
    if(fault_test_queue == NULL)
    {
        Log_Print(LOG_ERROR,"故障测试队列创建失败\r\n");
        while(1)
        {
            osal_msleep(100);
        }
    }

    //初始化故障管理模块，传入队列句柄
    ret = FaultMgr_Init(fault_test_queue);
    Log_Print(LOG_INFO,"故障管理模块初始化返回:%d\r\n", ret);

    //测试用例状态机
    enum
    {
        TEST_STEP_IDLE,
        TEST_STEP_TRIG_SENSOR_FAULT,    // 触发传感器故障
        TEST_STEP_CLEAR_SENSOR_FAULT,   // 清除传感器故障
        TEST_STEP_INVALID_CODE,         // 测试非法故障码
        TEST_STEP_FINISH
    } fault_test_step = TEST_STEP_IDLE;

    uint32_t case_tick = osal_get_ms();
    TickType_t last_wake_tick = osal_get_tick();

    while(1)
    {
        osal_delay_ms_until(&last_wake_tick, 20U);

        // 消费故障事件队列，打印模块投递出来的事件
        while(osal_queue_recv(fault_test_queue, &evt_buf, 0U) == 1U)
        {
            Log_Print(LOG_INFO,"收到故障事件 故障码:0x%02X 事件类型:%d\r\n",evt_buf.fault_code, evt_buf.event);
        }

        switch(fault_test_step)
        {
            case TEST_STEP_IDLE:
            {
                //等待2秒，切到第一个用例
                if((osal_get_ms() - case_tick) >= 2000U)
                {
                    fault_test_step = TEST_STEP_TRIG_SENSOR_FAULT;
                    case_tick = osal_get_ms();
                    Log_Print(LOG_INFO,"====开始：模拟传感器持续异常(0x01)，等待故障激活====\r\n");
                }
                //输入正常
                FaultMgr_InputStatus(FAULT_CODE_SENSOR, 1U);
                break;
            }

            case TEST_STEP_TRIG_SENSOR_FAULT:
            {
                //持续输入state=0异常，累计20次调用后故障激活
                FaultMgr_InputStatus(FAULT_CODE_SENSOR, 0U);

                //查询状态打印
                uint8_t state = FaultMgr_IsFaultActive(FAULT_CODE_SENSOR);
                Log_Print(LOG_INFO,"传感器故障激活状态:%d\r\n", state);

                //故障激活之后，进入清除测试
                if(state == 0U)
                {
                    Log_Print(LOG_INFO,"传感器故障已经激活，进入清除测试\r\n");
                    fault_test_step = TEST_STEP_CLEAR_SENSOR_FAULT;
                    case_tick = osal_get_ms();
                }
                break;
            }

            case TEST_STEP_CLEAR_SENSOR_FAULT:
            {
                //持续输入state=1正常，累计10次调用清除故障
                FaultMgr_InputStatus(FAULT_CODE_SENSOR, 1U);
                uint8_t state = FaultMgr_IsFaultActive(FAULT_CODE_SENSOR);
                Log_Print(LOG_INFO,"传感器故障激活状态:%d\r\n", state);

                if(state == 1U)
                {
                    Log_Print(LOG_INFO,"传感器故障已经清除，进入非法故障码用例\r\n");
                    fault_test_step = TEST_STEP_INVALID_CODE;
                    case_tick = osal_get_ms();
                }
                break;
            }

            case TEST_STEP_INVALID_CODE:
            {
                if(osal_get_ms() - case_tick < 500U)
                {
                    break;
                }
                //输入非法故障码，模块要打印错误日志，不能崩溃
                FaultMgr_InputStatus(0x10U, 0U);
                uint8_t ret_st = FaultMgr_IsFaultActive(0x10U);
                Log_Print(LOG_INFO,"非法故障码查询返回:%d\r\n", ret_st);

                fault_test_step = TEST_STEP_FINISH;
                case_tick = osal_get_ms();
                break;
            }

            case TEST_STEP_FINISH:
            default:
            {
                static uint8_t flag_print = 1U;
                if(flag_print)
                {
                    Log_Print(LOG_INFO,"====全部测试用例跑完，循环输入各路故障正常状态====\r\n");
                    flag_print = 0U;
                }
                FaultMgr_InputStatus(FAULT_CODE_SENSOR,1U);
                FaultMgr_InputStatus(FAULT_CODE_CAN,1U);
                FaultMgr_InputStatus(FAULT_CODE_LIGHT,1U);
                FaultMgr_InputStatus(FAULT_CODE_DOOR,1U);
                break;
            }
        }
    }
}

void Test_FaultMgr_CreateTask(void)
{
    osal_task_create(Test_FaultMgr_Task, "fault_mgr_test", 1024U, NULL, 2U, NULL);
}



static void Test_CanProtocol_Task(void *arg)
{
    (void)arg;

    CAN_Message_t rx_msg;
    CAN_Command_t cmd_out;
    uint8_t ret_parse;

    CanProtocol_Init();
    Log_Print(LOG_INFO,"==== CAN协议模块单元测试开始 ====\r\n");

    /* ---------------------- 空指针入参测试 ---------------------- */
    Log_Print(LOG_INFO,"1.解析接口空指针测试\r\n");
    ret_parse = CAN_Protocol_ParseMessage(NULL, &cmd_out);
    Log_Print(LOG_INFO,"rx_msg空指针，预期返回4，实际:%d\r\n", ret_parse);

    memset(&rx_msg,0,sizeof(rx_msg));
    ret_parse = CAN_Protocol_ParseMessage(&rx_msg, NULL);
    Log_Print(LOG_INFO,"p_cmd空指针，预期返回4，实际:%d\r\n", ret_parse);

    /* ---------------------- 不支持的ID测试 ---------------------- */
    Log_Print(LOG_INFO,"2.非法报文ID测试\r\n");
    memset(&rx_msg,0,sizeof(rx_msg));
    rx_msg.id = 0x555U;
    rx_msg.dlc = 1U;
    rx_msg.data[0] = 0x01U;
    ret_parse = CAN_Protocol_ParseMessage(&rx_msg, &cmd_out);
    Log_Print(LOG_INFO,"非法ID，预期返回2，实际:%d\r\n", ret_parse);

    /* ---------------------- DLC长度非法测试 ---------------------- */
    Log_Print(LOG_INFO,"3.DLC非法测试\r\n");
    memset(&rx_msg,0,sizeof(rx_msg));
    rx_msg.id = CAN_CMD_LIGHT;
    rx_msg.dlc = 2U;
    rx_msg.data[0] = 0x01U;
    ret_parse = CAN_Protocol_ParseMessage(&rx_msg, &cmd_out);
    Log_Print(LOG_INFO,"DLC非法，预期返回1，实际:%d\r\n", ret_parse);

    /* ---------------------- 信号值非法 ---------------------- */
    Log_Print(LOG_INFO,"4.信号值非法测试\r\n");
    memset(&rx_msg,0,sizeof(rx_msg));
    rx_msg.id = CAN_CMD_LIGHT;
    rx_msg.dlc = 1U;
    rx_msg.data[0] = 0x05U;
    ret_parse = CAN_Protocol_ParseMessage(&rx_msg, &cmd_out);
    Log_Print(LOG_INFO,"信号值非法，预期返回3，实际:%d\r\n", ret_parse);

    /* ---------------------- 灯光控制 合法报文解析 ---------------------- */
    Log_Print(LOG_INFO,"5.灯光控制合法报文解析\r\n");
    memset(&rx_msg,0,sizeof(rx_msg));
    rx_msg.id = CAN_CMD_LIGHT;
    rx_msg.dlc = 1U;
    rx_msg.data[0] = 0x01U;
    ret_parse = CAN_Protocol_ParseMessage(&rx_msg, &cmd_out);
    Log_Print(LOG_INFO,"灯光指令，预期返回0，实际:%d | command:%d value:%d\r\n",
                ret_parse, cmd_out.command, cmd_out.value);

    /* ---------------------- 门锁控制 合法报文解析 ---------------------- */
    Log_Print(LOG_INFO,"6.门锁控制合法报文解析\r\n");
    memset(&rx_msg,0,sizeof(rx_msg));
    rx_msg.id = CAN_CMD_LOCK;
    rx_msg.dlc = 1U;
    rx_msg.data[0] = 0x00U;
    ret_parse = CAN_Protocol_ParseMessage(&rx_msg, &cmd_out);
    Log_Print(LOG_INFO,"门锁指令，预期返回0，实际:%d | command:%d value:%d\r\n",
                ret_parse, cmd_out.command, cmd_out.value);

    /* ---------------------- 故障清除指令解析 ---------------------- */
    Log_Print(LOG_INFO,"7.故障清除指令解析\r\n");
    memset(&rx_msg,0,sizeof(rx_msg));
    rx_msg.id = CAN_CMD_FAULT_CLEAR;
    rx_msg.dlc = 1U;
    rx_msg.data[0] = 0x01U;
    ret_parse = CAN_Protocol_ParseMessage(&rx_msg, &cmd_out);
    Log_Print(LOG_INFO,"故障清除，预期返回0，实际:%d | command:%d value:%d\r\n",
                ret_parse, cmd_out.command, cmd_out.value);

    /* ---------------------- 报文发送测试 ---------------------- */
    Log_Print(LOG_INFO,"8.报文发送测试\r\n");
    {
        uint8_t send_ret;

        send_ret = CAN_Protocol_SendHeartbeat(0x01U);
        Log_Print(LOG_INFO,"发送心跳报文 ret=%d\r\n", send_ret);

        osal_msleep(20U);

        send_ret = CAN_Protocol_SendLightStatus(0x00U);
        Log_Print(LOG_INFO,"发送灯光状态报文 ret=%d\r\n", send_ret);

        osal_msleep(20U);

        send_ret = CAN_Protocol_SendDoorLockStatus(0x01U);
        Log_Print(LOG_INFO,"发送门锁状态报文 ret=%d\r\n", send_ret);

        osal_msleep(20U);

        send_ret = CAN_Protocol_SendFaultEvent(0x01U, 0x00U);
        Log_Print(LOG_INFO,"发送故障事件报文 ret=%d\r\n", send_ret);
    }

    Log_Print(LOG_INFO,"==== CAN协议解析、发送用例执行完毕，进入周期刷新硬件故障状态 ====\r\n");

    while(1)
    {
        CanProtocol_RefreshHwErrorState();
        uint8_t fault_flag = CAN_Protocol_GetRawFault();
        Log_Print(LOG_INFO,"CAN硬件故障标记 raw_fault_flag = %d\r\n", fault_flag);

        osal_msleep(1000U);
    }
}

void Test_CanProtocol_CreateTask(void)
{
    osal_task_create( Test_CanProtocol_Task,"can_proto_test",  1024, NULL, 2, NULL);
}




static osal_queue_t can_rx_test_queue = NULL;

static void Test_CanRx_LinkTask(void *arg)
{
    (void)arg;
    CAN_Message_t recv_msg;
    uint8_t parse_ret;
    CAN_Command_t cmd_out;

    can_rx_test_queue = osal_queue_create(8, sizeof(CAN_Message_t));
    if(can_rx_test_queue == NULL)
    {
        Log_Print(LOG_ERROR,"CAN接收测试：队列创建失败\r\n");
        while(1)
        {
            osal_msleep(100);
        }
    }

    CAN_Protocol_SetRxQueue(can_rx_test_queue);               //绑定接收回调
    Log_Print(LOG_INFO,"==== CAN接收中断链路测试启动 ====\r\n");
    Log_Print(LOG_INFO,"请上位机下发CAN指令帧，等待接收报文...\r\n");


    while(1)
    {
        //阻塞等待队列收到报文
        if(osal_queue_recv(can_rx_test_queue, &recv_msg, 2000) == 1U)
        {
            // 成功拿到来自中断回调投递过来的报文
            Log_Print(LOG_INFO,"[收到CAN报文] ID:0x%03X DLC:%d 时间戳:%lu\r\n",
                      recv_msg.id, recv_msg.dlc, recv_msg.timestamp);

            //打印原始数据 
            Log_Print(LOG_INFO,"报文数据: %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                      recv_msg.data[0],recv_msg.data[1],recv_msg.data[2],recv_msg.data[3],
                      recv_msg.data[4],recv_msg.data[5],recv_msg.data[6],recv_msg.data[7]);
            
            //解析报文
            parse_ret = CAN_Protocol_ParseMessage(&recv_msg, &cmd_out);
            if(parse_ret == 0U)
            {
                Log_Print(LOG_INFO,"报文解析成功 command:%d value:%d\r\n", cmd_out.command, cmd_out.value);
            }
            else
            {
                Log_Print(LOG_ERROR,"报文解析失败 ret=%d\r\n", parse_ret);
            }
        }
        else
        {
            // 队列超时，无报文，打印心跳提示
            Log_Print(LOG_INFO,"等待CAN报文中...\r\n");
        }
    }
}

void Test_CanRx_Link_CreateTask(void)
{
    osal_task_create(Test_CanRx_LinkTask, "can_rx_test", 1024U, NULL, 2U, NULL);
}




static osal_queue_t key_test_queue = NULL;

static void Test_KeyMgr_Task(void *arg)
{
    (void)arg;
    Key_Event_t key_evt;
    uint8_t ret;

    //创建按键事件队列
    key_test_queue = osal_queue_create(4, sizeof(Key_Event_t));
    if(key_test_queue == NULL)
    {
        Log_Print(LOG_ERROR,"按键测试：事件队列创建失败\r\n");
        while(1)
        {
            osal_msleep(100U);
        }
    }

    //初始化按键管理模块，传入队列句柄
    ret = KeyMgr_Init(key_test_queue);
    Log_Print(LOG_INFO,"==== 按键管理模块测试启动 ====\r\n");
    Log_Print(LOG_INFO,"KeyMgr_Init ret:%d\r\n", ret);
    Log_Print(LOG_INFO,"请按下 KEY1 / KEY2\r\n");

    TickType_t last_wake = osal_get_tick(); 

    while(1)
    {
        osal_delay_ms_until(&last_wake, 20U);
        KeyMgr_ScanProcess();

        if(osal_queue_recv(key_test_queue, &key_evt, 0U) == 1U)
        {
            switch(key_evt)
            {
                case KEY1_SHORT:
                    Log_Print(LOG_INFO,"按键事件：KEY1 短按\r\n");
                    break;
                case KEY1_LONG:
                    Log_Print(LOG_INFO,"按键事件：KEY1 长按\r\n");
                    break;
                case KEY2_SHORT:
                    Log_Print(LOG_INFO,"按键事件：KEY2 短按\r\n");
                    break;
                case KEY2_LONG:
                    Log_Print(LOG_INFO,"按键事件：KEY2 长按\r\n");
                    break;
                default:
                    Log_Print(LOG_ERROR,"按键事件：未知事件值 %d\r\n", key_evt);
                    break;
            }
        }

        //每500ms打印一次两路按键稳定电平
        static uint32_t log_tick = 0;
        if( (osal_get_ms() - log_tick) >= 500U )
        {
            log_tick = osal_get_ms();
            uint8_t st1 = KeyMgr_GetFilter(KEY_ID_1);
            uint8_t st2 = KeyMgr_GetFilter(KEY_ID_2);
            Log_Print(LOG_INFO,"[按键状态] KEY1:%d  KEY2:%d\r\n", st1, st2);
        }
    }
}

void Test_KeyMgr_CreateTask(void)
{
    osal_task_create(Test_KeyMgr_Task, "key_test", 1024U, NULL, 2U, NULL);
}





#define STORAGE_TEST_BUF_CNT    20U    //测试读取缓冲区最大条数

static void Test_StorageMgr_Task(void *arg)
{
    (void)arg;

    Fault_Record_t test_read_buf[STORAGE_TEST_BUF_CNT];
    uint16_t fill_cnt;              //循环写计数

    // 测试状态枚举 
    enum
    {
        STORAGE_TEST_IDLE,              //空闲，准备启动测试
        STORAGE_TEST_INIT,              //模块初始化
        STORAGE_TEST_WRITE_SINGLE,      //写入1条故障记录
        STORAGE_TEST_READ_CHECK,        //读取全部并校验
        STORAGE_TEST_WRITE_FILL_SECTOR, //循环写，填满扇区，触发扇区擦除
        STORAGE_TEST_CLEAR_ALL,         //清空全部故障日志
        STORAGE_TEST_FINISH             //全部测试完成
    } storage_test_step = STORAGE_TEST_IDLE;

    TickType_t last_wake = osal_get_tick();

    while(1)
    {
        //固定20ms周期
        osal_delay_ms_until(&last_wake, 20U);

        switch(storage_test_step)
        {
            case STORAGE_TEST_IDLE:
                //测试环境强制清空，避免下载后W25Q残留旧数据
                StorageMgr_ClearAllFault();
                osal_msleep(30);
                Log_Print(LOG_INFO,"=====存储管理模块测试开始=====\r\n");
                storage_test_step = STORAGE_TEST_INIT;
                break;

            case STORAGE_TEST_INIT:
            {
                uint8_t ret = StorageMgr_Init();
                Log_Print(LOG_INFO,"StorageMgr_Init 返回:%d\r\n", ret);
                if(ret == 1U)
                {
                    storage_test_step = STORAGE_TEST_WRITE_SINGLE;
                }
                else
                {
                    Log_Print(LOG_ERROR,"存储模块初始化失败，测试终止\r\n");
                    storage_test_step = STORAGE_TEST_FINISH;
                }
                break;
            }

            case STORAGE_TEST_WRITE_SINGLE:
            {
                Fault_Record_t rec;
                rec.fault_code = 0x01U;
                rec.status = 1U;
                rec.timestamp = osal_get_ms();

                uint8_t w_ret = StorageMgr_WriteFaultLog(&rec);
                Log_Print(LOG_INFO,"写入单条故障记录，返回=%d\r\n",w_ret);

                storage_test_step = STORAGE_TEST_READ_CHECK;
                break;
            }

            case STORAGE_TEST_READ_CHECK:
            {
                uint16_t valid = StorageMgr_ReadAllFault(test_read_buf, STORAGE_TEST_BUF_CNT);
                Log_Print(LOG_INFO,"读取到有效故障记录条数：%d\r\n",valid);

                for(uint16_t i=0; i<valid; i++)
                {
                    Log_Print(LOG_INFO,"  记录[%d]:故障码0x%02X,状态%d,时间戳%lu\r\n",
                        i,
                        test_read_buf[i].fault_code,
                        test_read_buf[i].status,
                        test_read_buf[i].timestamp);
                }

                Log_Print(LOG_INFO,"准备执行填满扇区测试\r\n");
                fill_cnt = 0U;
                storage_test_step = STORAGE_TEST_WRITE_FILL_SECTOR;
                break;
            }

            case STORAGE_TEST_WRITE_FILL_SECTOR:
            {
                Fault_Record_t rec;
                rec.fault_code = 0x02U;
                rec.status = (fill_cnt % 2U);    //状态交替0/1
                rec.timestamp = osal_get_ms();

                uint8_t w_ret = StorageMgr_WriteFaultLog(&rec);
                if(w_ret != 1U)
                {
                    Log_Print(LOG_ERROR,"写记录失败，count=%d\r\n",fill_cnt);
                }
                fill_cnt++;

                if(fill_cnt >= 100U)  //写满100条记录，触发扇区擦除
                {
                    Log_Print(LOG_INFO,"填满扇区测试完成，写入总次数=%d\r\n",fill_cnt);
                    storage_test_step = STORAGE_TEST_CLEAR_ALL;
                }
                break;
            }

            case STORAGE_TEST_CLEAR_ALL:
            {
                uint8_t clr_ret = StorageMgr_ClearAllFault();
                Log_Print(LOG_INFO,"执行清空全部故障记录，返回=%d\r\n",clr_ret);

                //清空后再读，确认记录数量为0
                uint16_t after_clear = StorageMgr_ReadAllFault(test_read_buf, STORAGE_TEST_BUF_CNT);
                Log_Print(LOG_INFO,"清空后读取记录条数 = %d（预期0）\r\n", after_clear);

                storage_test_step = STORAGE_TEST_FINISH;
                break;
            }

            case STORAGE_TEST_FINISH:
                Log_Print(LOG_INFO,"====存储管理模块全部测试用例跑完====\r\n");
                osal_msleep(2000);
                break;

            default:
                storage_test_step = STORAGE_TEST_IDLE;
                break;
        }
    }
}

void Test_StorageMgr_CreateTask(void)
{
    osal_task_create(Test_StorageMgr_Task, "storage_test", 768, NULL, 2, NULL);
}



static void Test_ExecutorDiag_Task(void *arg)
{
    (void)arg;

    ExecutorDiag_Init();
    Log_Print(LOG_INFO,"===== 执行器诊断测试任务启动 =====\r\n");

    enum
    {
        TEST_STEP_IDLE,

        TEST_STEP_LIGHT_SET_EXP1,      // 设置灯光期望=1 
        TEST_STEP_LIGHT_WAIT1,         //等待采样 
        TEST_STEP_LIGHT_CHECK1,        //校验用例1结果 

        TEST_STEP_LIGHT_SET_EXP0,     
        TEST_STEP_LIGHT_WAIT0,
        TEST_STEP_LIGHT_CHECK0,        

        TEST_STEP_LOCK_SET_EXP1,      
        TEST_STEP_LOCK_WAIT1,
        TEST_STEP_LOCK_CHECK1,        

        TEST_STEP_LOCK_SET_EXP0,       
        TEST_STEP_LOCK_WAIT0,
        TEST_STEP_LOCK_CHECK0,      

        TEST_STEP_ALL_DONE            //全部用例跑完，常驻打印状态
    } diag_test_step = TEST_STEP_LIGHT_SET_EXP1;

    uint32_t step_start_ms = 0U;
    TickType_t last_wake = osal_get_tick();

    step_start_ms = osal_get_ms();

    while(1)
    {
        osal_delay_ms_until(&last_wake, 10U);

        ExecutorDiag_LightRoutine();
        ExecutorDiag_LockRoutine();

        uint8_t light_raw_fault = ExecutorDiag_GetLightRawFault();
        uint8_t lock_raw_fault  = ExecutorDiag_GetLockRawFault();
 
        switch(diag_test_step)
        {
        case TEST_STEP_LIGHT_SET_EXP1:
        {
            ExecutorDiag_SetLightExpect(1U);
            Log_Print(LOG_INFO,"1.灯光期望=1，预期raw_fault=0\r\n");
            step_start_ms = osal_get_ms();
            diag_test_step = TEST_STEP_LIGHT_WAIT1;
            break;
        }
        case TEST_STEP_LIGHT_WAIT1:
        {
            if(osal_get_ms() - step_start_ms < 1000U)
            {
                break;
            }
            diag_test_step = TEST_STEP_LIGHT_CHECK1;
            break;
        }
        case TEST_STEP_LIGHT_CHECK1:
        {
            Log_Print(LOG_INFO,"light_raw_fault：预期0，实际=%d\r\n", light_raw_fault);
            diag_test_step = TEST_STEP_LIGHT_SET_EXP0;
            step_start_ms = osal_get_ms();
            break;
        }

        case TEST_STEP_LIGHT_SET_EXP0:
        {
            ExecutorDiag_SetLightExpect(0U);
            Log_Print(LOG_INFO,"2.灯光期望=0，预期raw_fault=1\r\n");
            step_start_ms = osal_get_ms();
            diag_test_step = TEST_STEP_LIGHT_WAIT0;
            break;
        }
        case TEST_STEP_LIGHT_WAIT0:
        {
            if(osal_get_ms() - step_start_ms < 1000U)
            {
                break;
            }
            diag_test_step = TEST_STEP_LIGHT_CHECK0;
            break;
        }
        case TEST_STEP_LIGHT_CHECK0:
        {
            Log_Print(LOG_INFO,"light_raw_fault：预期1，实际=%d\r\n", light_raw_fault);
            diag_test_step = TEST_STEP_LOCK_SET_EXP1;
            step_start_ms = osal_get_ms();
            break;
        }

        case TEST_STEP_LOCK_SET_EXP1:
        {
            ExecutorDiag_SetLockExpect(1U);
            Log_Print(LOG_INFO,"3.门锁期望=1，预期raw_fault=0\r\n");
            step_start_ms = osal_get_ms();
            diag_test_step = TEST_STEP_LOCK_WAIT1;
            break;
        }
        case TEST_STEP_LOCK_WAIT1:
        {
            if(osal_get_ms() - step_start_ms < 1000U)
            {
                break;
            }
            diag_test_step = TEST_STEP_LOCK_CHECK1;
            break;
        }
        case TEST_STEP_LOCK_CHECK1:
        {
            Log_Print(LOG_INFO,"lock_raw_fault：预期0，实际=%d\r\n", lock_raw_fault);
            diag_test_step = TEST_STEP_LOCK_SET_EXP0;
            step_start_ms = osal_get_ms();
            break;
        }

        case TEST_STEP_LOCK_SET_EXP0:
        {
            ExecutorDiag_SetLockExpect(0U);
            Log_Print(LOG_INFO,"4.门锁期望=0，预期raw_fault=1\r\n");
            step_start_ms = osal_get_ms();
            diag_test_step = TEST_STEP_LOCK_WAIT0;
            break;
        }
        case TEST_STEP_LOCK_WAIT0:
        {
            if(osal_get_ms() - step_start_ms < 1000U)
            {
                break;
            }
            diag_test_step = TEST_STEP_LOCK_CHECK0;
            break;
        }
        case TEST_STEP_LOCK_CHECK0:
        {
            Log_Print(LOG_INFO,"lock_raw_fault：预期1，实际=%d\r\n", lock_raw_fault);
            Log_Print(LOG_INFO,"===== 全部执行器诊断用例执行完毕 =====\r\n");
            diag_test_step = TEST_STEP_ALL_DONE;
            step_start_ms = osal_get_ms();
            break;
        }

        case TEST_STEP_ALL_DONE:
        {
            if(osal_get_ms() - step_start_ms >= 2000U)
            {
                Log_Print(LOG_INFO,"light_raw_fault:%d  lock_raw_fault:%d\r\n",
                          light_raw_fault, lock_raw_fault);
                step_start_ms = osal_get_ms();
            }
            break;
        }

        case TEST_STEP_IDLE:
        default:
            break;
        }
    }
}

void Test_ExecutorDiag_CreateTask(void)
{
    osal_task_create(Test_ExecutorDiag_Task,"test_diag",1024U,NULL,2U,NULL);
}


#endif