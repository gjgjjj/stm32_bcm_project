#ifndef __SYSTEM_MANAGER_H
#define __SYSTEM_MANAGER_H

#include <stdint.h>


//系统总初始化    
void SystemMgr_Init(void);

void SystemMgr_StartTasksAndTimer(void);

void can_protocol_CreateTask(void);     //CAN协议任务
void fault_monitor_CreateTask(void);    //故障监控任务
void input_collect_CreateTask(void);    //输入采集任务
void bcm_control_CreateTask(void);      //BCM控制任务
void system_manage_CreateTask(void);    //系统管理任务
void fault_process_CreateTask(void);    //故障处理任务

#endif
