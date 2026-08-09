#include "bsp_flash.h"

extern SPI_HandleTypeDef hspi1;

#define FLASH_CS_GPIO_PORT      GPIOA
#define FLASH_CS_GPIO_PIN       GPIO_PIN_4

#define FLASH_CS_LOW()      HAL_GPIO_WritePin(FLASH_CS_GPIO_PORT,FLASH_CS_GPIO_PIN,GPIO_PIN_RESET)
#define FLASH_CS_HIGH()     HAL_GPIO_WritePin(FLASH_CS_GPIO_PORT,FLASH_CS_GPIO_PIN,GPIO_PIN_SET)

#define W25Q_CMD_WRITE_ENABLE       0x06U   // 写使能命令
#define W25Q_CMD_READ_STATUS        0x05U   // 读状态寄存器，bit0=1 芯片忙
#define W25Q_CMD_READ_DATA          0x03U   // 读数据
#define W25Q_CMD_PAGE_PROGRAM       0x02U   // 页写入(最多256字节)
#define W25Q_CMD_SECTOR_ERASE       0x20U   // 4KB扇区擦除

#define FLASH_TIMEOUT               1000U      // 操作超时时间ms


static uint8_t FLASH_SPI_SendByte(uint8_t data, BSP_Flash_StatusTypeDef *status_p)
{
    uint8_t rx = 0;

    if(HAL_SPI_TransmitReceive(&hspi1, &data, &rx, 1, FLASH_TIMEOUT) != HAL_OK)
    {
        *status_p = BSP_FLASH_ERR_SPI;
        return 0;
    }
    *status_p = BSP_FLASH_OK;
    return rx;
}

static BSP_Flash_StatusTypeDef FLASH_WriteEnable(void)
{
    BSP_Flash_StatusTypeDef status;           //接收FLASH_SPI_SendByte()输出的错误状态

    FLASH_CS_LOW();
    FLASH_SPI_SendByte(W25Q_CMD_WRITE_ENABLE, &status);
    FLASH_CS_HIGH();

    return status;
}

// 等待 Flash 空闲
static BSP_Flash_StatusTypeDef FLASH_WaitBusy(void)
{
    uint32_t tick = HAL_GetTick();   // 记录起始时间
    BSP_Flash_StatusTypeDef status;
    uint8_t state;
    do
    {
        FLASH_CS_LOW();
        FLASH_SPI_SendByte(W25Q_CMD_READ_STATUS, &status);    // 发送读状态寄存器命令
        if(status != BSP_FLASH_OK)
        {
            FLASH_CS_HIGH();
            return status;
        }

        //W25Q 接收 0x05 命令后,下一个 SPI 时钟输出状态值,全双工收发同步，发送 0xFF 哑字节读取返回数据
        state = FLASH_SPI_SendByte(0xFF, &status);   // 读取状态寄存器值
        FLASH_CS_HIGH();

        if(status != BSP_FLASH_OK)                   //读状态寄存器收发出错，直接返回错误
        {
            return status;
        }

        if((state & 0x01U) == 0U)                    // bit0=0：CPU空闲,写/擦除已经完成，函数返回 OK
        {
            return BSP_FLASH_OK;
        }

        if((HAL_GetTick() - tick) > FLASH_TIMEOUT)   // 等待时间 > 1000ms
        {
            return BSP_FLASH_ERR_TIMEOUT;
        }
    }while(1);                                       //return时跳出循环
}



// Flash硬件初始化
BSP_Flash_StatusTypeDef BSP_FLASH_Init(void)
{
    FLASH_CS_HIGH();           //默认CS拉高，不选中Flash芯片
    return BSP_FLASH_OK;
}

//Flash连续读取数据
BSP_Flash_StatusTypeDef BSP_FLASH_Read(uint32_t addr, uint8_t *buf, uint16_t len)
{
    BSP_Flash_StatusTypeDef status;

    if(buf == NULL || len == 0U)
    {
        return BSP_FLASH_ERR_PARAM;
    }

    FLASH_CS_LOW();

    FLASH_SPI_SendByte(W25Q_CMD_READ_DATA, &status);
    if(status != BSP_FLASH_OK)    //出错直接跳到出口
        goto READ_EXIT;     

    //24位地址拆分，先发高8位
    FLASH_SPI_SendByte((addr >> 16) & 0xffU, &status);
    FLASH_SPI_SendByte((addr >> 8)  & 0xffU, &status);
    FLASH_SPI_SendByte(addr & 0xffU, &status);

    //循环发送0xFF哑字节，返回数据存入buf
    for(uint16_t i = 0U; i < len; i++)
    {
        buf[i] = FLASH_SPI_SendByte(0xffU, &status);
        if(status != BSP_FLASH_OK)
            goto READ_EXIT;
    }

READ_EXIT:
    FLASH_CS_HIGH();
    return status;
}

// 页写入
BSP_Flash_StatusTypeDef BSP_FLASH_WritePage(uint32_t addr, uint8_t *buf, uint16_t len)
{
    BSP_Flash_StatusTypeDef status;

    if(buf == NULL || len == 0U || len > FLASH_PAGE_SIZE)
    {
        return BSP_FLASH_ERR_PARAM;
    }

    if(((addr & 0xffU) + len) > FLASH_PAGE_SIZE)  //判断是否跨页边界
    {
        return BSP_FLASH_ERR_PARAM;
    }

    // 等待芯片空闲
    status = FLASH_WaitBusy();
    if(status != BSP_FLASH_OK)
        return status;

    //写使能
    status = FLASH_WriteEnable();
    if(status != BSP_FLASH_OK)
        return status;

    FLASH_CS_LOW();
    FLASH_SPI_SendByte(W25Q_CMD_PAGE_PROGRAM, &status);      //页写命令
    FLASH_SPI_SendByte((addr >> 16) & 0xffU, &status);       //24bit地址，高8位
    FLASH_SPI_SendByte((addr >> 8)  & 0xffU, &status);    
    FLASH_SPI_SendByte(addr & 0xffU, &status);             

    //循环发送要写入的数据
    for(uint16_t i = 0U; i < len; i++)
    {
        FLASH_SPI_SendByte(buf[i], &status);
        if(status != BSP_FLASH_OK)                    //任意一次 SPI 收发出错，立刻跳出循环
            goto WRITE_EXIT;
    }

WRITE_EXIT:
    FLASH_CS_HIGH();                                 //CS 拉高，触发Flash内部烧写
    if(status != BSP_FLASH_OK)
        return status;


    return FLASH_WaitBusy();                         //循环读 W25Q 状态寄存器,返回状态，等待烧写完成
}

//Flash扇区擦除
BSP_Flash_StatusTypeDef BSP_FLASH_EraseSector(uint32_t addr)
{
    BSP_Flash_StatusTypeDef status;

    //地址对齐校验
    if((addr & 0xFFFU) != 0U)
    {
        return BSP_FLASH_ERR_PARAM;
    }

    status = FLASH_WaitBusy();
    if(status != BSP_FLASH_OK)
        return status;

    status = FLASH_WriteEnable();
    if(status != BSP_FLASH_OK)
        return status;

    FLASH_CS_LOW();
    FLASH_SPI_SendByte(W25Q_CMD_SECTOR_ERASE, &status);
    FLASH_SPI_SendByte((addr >> 16) & 0xffU, &status);
    FLASH_SPI_SendByte((addr >> 8)  & 0xffU, &status);
    FLASH_SPI_SendByte(addr & 0xffU, &status);

    FLASH_CS_HIGH();

    if(status != BSP_FLASH_OK)
        return status;

    // 等待擦除操作完成
    return FLASH_WaitBusy();
}


