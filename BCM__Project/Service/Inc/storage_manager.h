#ifndef __STORAGE_MANAGER_H
#define __STORAGE_MANAGER_H

#include <stdint.h>

// Flash存储单条故障日志记录
typedef struct
{
    uint8_t  fault_code;
    uint8_t  status;
    uint32_t timestamp;
    uint8_t  checksum;
} Fault_Record_t;


//存储模块初始化，加载环形日志到内存缓存，校验历史记录
uint8_t StorageMgr_Init(void);


//写入一条故障记录到Flash环形日志区
uint8_t StorageMgr_WriteFaultLog(const Fault_Record_t *rec);


//读取所有有效的历史故障日志    返回读取到有效故障记录条数
//*buf  调用方创建的内存数组      max_cnt 数组最大可存故障条数
uint16_t StorageMgr_ReadAllFault(Fault_Record_t *buf, uint16_t max_cnt);


//全部清空故障日志环形区
uint8_t StorageMgr_ClearAllFault(void);

//打印历史故障日志
void StorageMgr_PrintAllFaultLog(void);

#endif
