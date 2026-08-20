#include "storage_manager.h"
#include "bsp_flash.h"
#include <stdint.h>
#include <string.h>


#define STORAGE_LOG_START_ADDR      0x001000U    //故障日志起始Flash地址，4KB扇区
#define STORAGE_LOG_SECTOR_SIZE     4096U
#define STORAGE_RECORD_LEN          7U           //单条记录序列化字节长度
#define STORAGE_MAX_RECORD_CNT      (STORAGE_LOG_SECTOR_SIZE / STORAGE_RECORD_LEN)

static uint16_t write_index;            //下一条要写入的记录下标 0 ~ STORAGE_MAX_RECORD_CNT‑1
static uint8_t storage_init = 0;        //模块初始化标志


//单条故障记录和校验
static uint8_t StorageMgr_Checksum(const Fault_Record_t *rec)
{
    uint8_t sum = 0U;

    sum += rec->fault_code;

    sum += rec->status;

    sum += (rec->timestamp >> 0) & 0xFFU;
    sum += (rec->timestamp >> 8) & 0xFFU;
    sum += (rec->timestamp >> 16) & 0xFFU;
    sum += (rec->timestamp >> 24) & 0xFFU;
    return sum;
}

//结构体 → 字节数组序列化
static void StorageMgr_Serialize(const Fault_Record_t *rec, uint8_t *out_buf)
{
    out_buf[0] = rec->fault_code;
    out_buf[1] = rec->status;
    out_buf[2] = (rec->timestamp >> 0) & 0xFFU;
    out_buf[3] = (rec->timestamp >> 8) & 0xFFU;
    out_buf[4] = (rec->timestamp >> 16) & 0xFFU;
    out_buf[5] = (rec->timestamp >> 24) & 0xFFU;
    out_buf[6] = rec->checksum;
}

//字节数组 → 结构体反序列化
static void StorageMgr_Deserialize(const uint8_t *in_buf, Fault_Record_t *rec)
{
    rec->fault_code  = in_buf[0];
    rec->status      = in_buf[1];
    rec->timestamp   = ((uint32_t)in_buf[5] <<24U) | ((uint32_t)in_buf[4] <<16U)
                     | ((uint32_t)in_buf[3] <<8U)  | ((uint32_t)in_buf[2]);
    rec->checksum    = in_buf[6];
}

//上电扫描Flash，定位空闲槽位,定位写指针write_index
static uint8_t StorageMgr_LoadFromFlash(void)
{
    Fault_Record_t temp_rec;
    uint8_t buf[STORAGE_RECORD_LEN];

    write_index = 0U;
    uint16_t i;
    for(i = 0U; i < STORAGE_MAX_RECORD_CNT; i++)
    {
        //计算当前槽位对应的Flash物理地址
        uint32_t addr = STORAGE_LOG_START_ADDR + i * STORAGE_RECORD_LEN;
        BSP_Flash_StatusTypeDef ret = BSP_FLASH_Read(addr, buf, STORAGE_RECORD_LEN);
        if(ret != BSP_FLASH_OK)
        {
            continue;              //读Flash失败，跳过本槽，继续扫描下一个
        }

        //判断是否空闲槽
        uint8_t empty = 1U;
        for(uint8_t k = 0; k < STORAGE_RECORD_LEN; k++)
        {
            if(buf[k] != 0xFFU)
            {
                empty = 0U;
                break;
            }
        }
        if(empty != 0U)
        {
            write_index = i;        //找到空闲槽，写指针赋值为当前下标，结束扫描
            break;
        }

        StorageMgr_Deserialize(buf, &temp_rec);
        uint8_t check_sum = StorageMgr_Checksum(&temp_rec);
        (void)check_sum;
    }

    //for循环全部跑完，代表所有槽全部被占用，写指针置0，准备擦除重写
    if(i >= STORAGE_MAX_RECORD_CNT)
    {
        write_index = 0U;
    }
    return 1;
}

//必须早于故障消费任务启动
uint8_t StorageMgr_Init(void)
{
    BSP_Flash_StatusTypeDef bsp_ret = BSP_FLASH_Init();
    if(bsp_ret != BSP_FLASH_OK)
    {
        storage_init = 0;
        return 0;
    }
    uint8_t load_ret = StorageMgr_LoadFromFlash();
    if(load_ret != 1)
    {
        storage_init = 0;
        return 0;
    }
    storage_init = 1;
    return 1;
}


//写入一条故障记录到Flash日志
uint8_t StorageMgr_WriteFaultLog(const Fault_Record_t *rec)
{
    if((!storage_init) || (rec == NULL))
    {
        return 0;
    }

    if(write_index >= STORAGE_MAX_RECORD_CNT)               //保护下标越界
    {
        write_index = 0U;
    }

    Fault_Record_t local_rec = *rec;
    local_rec.checksum = StorageMgr_Checksum(&local_rec);   //本地计算校验和

    uint8_t write_buf[STORAGE_RECORD_LEN];
    StorageMgr_Serialize(&local_rec, write_buf);

    uint32_t write_addr = STORAGE_LOG_START_ADDR + write_index * STORAGE_RECORD_LEN;  //计算待写入Flash地址

    uint8_t need_erase = 0;
    if(write_index == 0U)                                   //准备写第0号槽，判断是否需要擦除扇区
    {
        uint8_t first_byte;
        if(BSP_FLASH_Read(STORAGE_LOG_START_ADDR, &first_byte, 1U) == BSP_FLASH_OK)
        {
            if(first_byte != 0xFFU)
            {
                need_erase = 1;                             //扇区存在旧数据，需要擦除
            }
        }
        else
        {
            need_erase = 1;                                 //读失败，保守处理执行擦除
        }
    }

    if(need_erase == 1)
    {
        BSP_Flash_StatusTypeDef erase_ret = BSP_FLASH_EraseSector(STORAGE_LOG_START_ADDR);
        if(erase_ret != BSP_FLASH_OK)
        {
            return 0;                                       //扇区擦除失败，直接返回，不执行写入
        }
    }

    BSP_Flash_StatusTypeDef write_ret = BSP_FLASH_WritePage(write_addr, write_buf, STORAGE_RECORD_LEN);
    if(write_ret != BSP_FLASH_OK)
    {
        return 0;
    }

    write_index ++;                                        //写成功，写指针向后移动一个槽位
    if(write_index >= STORAGE_MAX_RECORD_CNT)
    {
        write_index = 0U;                                  //下标重置，下一次写入触发擦除
    }
    return 1;
}


//读取全部合法故障记录
uint16_t StorageMgr_ReadAllFault(Fault_Record_t *buf, uint16_t max_cnt)
{
    if((!storage_init) || (buf == NULL) || (max_cnt == 0U))
    {
        return 0;
    }
    uint16_t valid_out_cnt =0U;                  //有效记录条数
    uint8_t temp_buf[STORAGE_RECORD_LEN];

    for(uint16_t i = 0U; i < STORAGE_MAX_RECORD_CNT; i++)
    {
        if(valid_out_cnt >= max_cnt)             //输出缓冲区存满，退出
        {
            break;
        }
        uint32_t addr = STORAGE_LOG_START_ADDR + i * STORAGE_RECORD_LEN;
        BSP_Flash_StatusTypeDef ret = BSP_FLASH_Read(addr, temp_buf, STORAGE_RECORD_LEN);
        if(ret != BSP_FLASH_OK)
        {
            continue;                            //单条读失败跳过，继续读下一条
        }

        // 全字节为0xFF才判定为空槽，结束读取
        uint8_t empty = 1U;
        for(uint8_t k = 0; k < STORAGE_RECORD_LEN; k++)
        {
            if(temp_buf[k] != 0xFFU)
            {
                empty = 0U;
                break;
            }
        }
        if(empty != 0U)
        {
            break;
        }

        Fault_Record_t tmp_rec;
        StorageMgr_Deserialize(temp_buf, &tmp_rec);
        uint8_t check_sum = StorageMgr_Checksum(&tmp_rec);
        if(check_sum == tmp_rec.checksum)
        {
            buf[valid_out_cnt] = tmp_rec;
            valid_out_cnt ++;
        }
    }
    return valid_out_cnt;
}

//清除全部故障记录
uint8_t StorageMgr_ClearAllFault(void)
{
    if(!storage_init)
    {
        return 0;
    }
    BSP_Flash_StatusTypeDef ret = BSP_FLASH_EraseSector(STORAGE_LOG_START_ADDR);
    if(ret != BSP_FLASH_OK)
    {
        return 0;
    }
    write_index = 0U;                  //擦除成功，写指针复位
    return 1;
}


