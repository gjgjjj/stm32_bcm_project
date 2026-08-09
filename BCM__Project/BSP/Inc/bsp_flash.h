#ifndef __BSP_FLASH_H
#define __BSP_FLASH_H

#include "stm32f1xx_hal.h"

//返回状态枚举
typedef enum
{
    BSP_FLASH_OK = 0U,          // Flash操作执行成功
    BSP_FLASH_ERR_SPI,          // SPI通信错误，SPI硬件收发异常
    BSP_FLASH_ERR_TIMEOUT,      // 操作超时，Flash硬件应答超时、等待忙标志超时
    BSP_FLASH_ERR_PARAM,        // 传入参数非法

} BSP_Flash_StatusTypeDef;

BSP_Flash_StatusTypeDef BSP_FLASH_Init(void);                                           //SPI外设与Flash初始化
BSP_Flash_StatusTypeDef BSP_FLASH_Read(uint32_t addr, uint8_t *buf, uint16_t len);      //读数据
BSP_Flash_StatusTypeDef BSP_FLASH_WritePage(uint32_t addr, uint8_t *buf, uint16_t len); //页写入
BSP_Flash_StatusTypeDef BSP_FLASH_EraseSector(uint32_t addr);                           //擦除指定扇区

#endif

