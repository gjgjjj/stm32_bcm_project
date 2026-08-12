#include "bsp_test.h"
#include <string.h>

#include "bsp_gpio.h"
#include "bsp_adc.h"
#include "bsp_can.h"
#include "bsp_flash.h"
#include "bsp_uart.h"
#include "bsp_rtc.h"

#include "FreeRTOS.h"
#include "task.h"


#define TEST_GPIO_OUT        RGB_RED           // 测试用输出引脚ID
#define TEST_GPIO_IN         KEY1               // 测试用输入引脚ID
#define TEST_FLASH_SECTOR    0x000F0000UL      // Flash测试扇区地址
#define TEST_CAN_STD_ID      0x123U            // CAN测试报文标准ID
#define TEST_LOOP_CNT        5U                // 测试循环次数

#define BSP_TEST_OK          1U
#define BSP_TEST_FAIL        0U


void Test_UART(void);
void Test_GPIO(void);       
void Test_ADC(void);
void Test_RTC(void);
void Test_Flash(void);
void Test_CAN(void);


void BSP_UnitTestTask(void *argument)
{
    (void)argument;

    // size_t heap_free = xPortGetFreeHeapSize();  //获取 RTOS 堆当前剩余可用字节数
    // (void)heap_free;

    UART_Printf("======================================");
    UART_Printf("\r\n        BSP 底层驱动单元测试        ");
    UART_Printf("\r\n======================================\r\n");

    Test_UART();
    Test_GPIO();
    Test_ADC();
    Test_RTC();
    Test_Flash();
    Test_CAN();

    UART_Printf("\r\n======================================");
    UART_Printf("\r\n        全部测试项执行完毕        ");
    UART_Printf("\r\n======================================\r\n");
    UART_Printf("测试任务释放资源，自我删除\r\n\r\n");

    /* 测试完成，销毁当前任务 */
    osThreadTerminate(osThreadGetId());
}


void Test_UART(void)
{
    UART_Printf("\r\n--- UART 模块测试 ---\r\n");

    /* 正常用例1：字符串发送 */
    UART_SendString("UART_SendString 正常输出测试\r\n");

    /* 正常用例2：格式化打印 */
    UART_Printf("UART_Printf 格式化测试: 数字=%d, 十六进制=0x%02X\r\n", 123, 0xAB);

    /* 异常用例：空指针入参，验证驱动参数防护 */
    UART_SendString(NULL);
    UART_Printf(NULL);
    UART_Printf("异常入参测试完成，未死机则防护生效\r\n");

    UART_Printf("UART 测试通过\r\n");
}


void Test_GPIO(void)
{
    uint8_t read_val;

    UART_Printf("\r\n--- GPIO 模块测试 ---\r\n");

    /* 正常用例1：输出引脚置有效 */
    GPIO_WritePin(TEST_GPIO_OUT, 1);
    read_val = GPIO_ReadPin(TEST_GPIO_OUT);
    UART_Printf("输出置1后读取: %d\r\n", read_val);
    vTaskDelay(pdMS_TO_TICKS(500));

    /* 正常用例2：输出引脚置无效 */
    GPIO_WritePin(TEST_GPIO_OUT, 0);
    read_val = GPIO_ReadPin(TEST_GPIO_OUT);
    UART_Printf("输出置0后读取: %d\r\n", read_val);
    vTaskDelay(pdMS_TO_TICKS(500));

    /* 正常用例3：引脚翻转 */
    UART_Printf("引脚连续翻转 %d 次\r\n", TEST_LOOP_CNT);
    for(uint8_t i = 0; i < TEST_LOOP_CNT; i++)
    {
        GPIO_TogglePin(TEST_GPIO_OUT);
        vTaskDelay(pdMS_TO_TICKS(300));
    }

    /* 正常用例4：输入引脚读取 */
    read_val = GPIO_ReadPin(TEST_GPIO_IN);
    UART_Printf("输入引脚当前状态: %d\r\n", read_val);

    /* 异常用例：传入非法ID，验证驱动不崩溃 */
    GPIO_WritePin(0xFF, 1);
    GPIO_ReadPin(0xFF);
    GPIO_TogglePin(0xFF);

    UART_Printf("GPIO 测试通过（肉眼确认输出引脚动作是否正常）\r\n");
}


void Test_ADC(void)
{
    uint16_t adc_val;

    UART_Printf("\r\n--- ADC 模块测试 ---\r\n");

    UART_Printf("连续采样 %d 次原始值:\r\n", TEST_LOOP_CNT);
    for(uint8_t i = 0; i < TEST_LOOP_CNT; i++)
    {
        adc_val = ADC_GetValue();

        /* 数值合理性校验：12位ADC范围 0~4095 */
        if(adc_val > 3000)
        {
            UART_Printf("第%d次采样值超出范围: %d\r\n", i+1, adc_val);
        }
        else
        {
            UART_Printf("  第%d次: %d\r\n", i+1, adc_val);
        }
        vTaskDelay(pdMS_TO_TICKS(300));
    }

    UART_Printf("ADC 测试通过\r\n");
}


void Test_RTC(void)
{
    uint32_t ts_prev, ts_curr;
    uint8_t test_fail = 0U;

    UART_Printf("\r\n--- RTC 模块测试 ---\r\n");

    /* 预读一次，丢弃初始化后不稳定的第一个值 */
    (void)BSP_RTC_GetTimestamp();
    ts_prev = BSP_RTC_GetTimestamp();
    UART_Printf("初始时间戳: %lu s\r\n", ts_prev);

    UART_Printf("每秒读取一次，共 %d 次:\r\n", TEST_LOOP_CNT);
    for(uint8_t i = 0U; i < TEST_LOOP_CNT; i++)
    {
        vTaskDelay(pdMS_TO_TICKS(1000U));
        ts_curr = BSP_RTC_GetTimestamp();
        UART_Printf("  第%d秒: %lu s\r\n", i+1, ts_curr);

        /* i=0 是第一次采样，跳过校验；从第二次开始强制校验递增 */
        if(i > 0U)
        {
            if(ts_curr <= ts_prev)
            {
                UART_Printf("时间戳未正常递增\r\n");
                test_fail = 1U;
            }
        }
        ts_prev = ts_curr;
    }

    if(test_fail != 0U)
    {
        UART_Printf("RTC 测试失败\r\n");
    }
    else
    {
        UART_Printf("RTC 测试通过\r\n");
    }
}


void Test_Flash(void)
{
    BSP_Flash_StatusTypeDef ret;
    uint8_t write_buf[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    uint8_t read_buf[8] = {0};

    UART_Printf("\r\n--- SPI Flash 模块测试 ---\r\n");

    /* 正常用例1：扇区擦除 */
    UART_Printf("擦除扇区 0x%08lX ...\r\n", TEST_FLASH_SECTOR);
    ret = BSP_FLASH_EraseSector(TEST_FLASH_SECTOR);
    if(ret != BSP_FLASH_OK)
    {
        UART_Printf("扇区擦除失败，错误码: %d\r\n", ret);
        return;
    }
    UART_Printf("扇区擦除成功\r\n");

    /* 正常用例2：页写入 */
    UART_Printf("写入 %d 字节数据 ...\r\n", sizeof(write_buf));
    ret = BSP_FLASH_WritePage(TEST_FLASH_SECTOR, write_buf, sizeof(write_buf));
    if(ret != BSP_FLASH_OK)
    {
        UART_Printf("页写入失败，错误码: %d\r\n", ret);
        return;
    }
    UART_Printf("页写入成功\r\n");

    /* 正常用例3：读取并校验数据 */
    ret = BSP_FLASH_Read(TEST_FLASH_SECTOR, read_buf, sizeof(read_buf));
    if(ret != BSP_FLASH_OK)
    {
        UART_Printf("读取失败，错误码: %d\r\n", ret);
        return;
    }

    if(memcmp(write_buf, read_buf, sizeof(write_buf)) == 0)
    {
        UART_Printf("数据校验一致，读写正常\r\n");
    }
    else
    {
        UART_Printf("读写数据不一致\r\n");
        UART_Printf("写入: ");
        for(uint8_t i=0; i<sizeof(write_buf); i++) 
        UART_Printf("%02X ", write_buf[i]);

        UART_Printf("\r\n读出: ");
        for(uint8_t i=0; i<sizeof(read_buf); i++) 
        UART_Printf("%02X ", read_buf[i]);

        UART_Printf("\r\n");
    }

    /* 异常用例：非法入参校验 */
    UART_Printf("异常入参测试...\r\n");
    ret = BSP_FLASH_Read(TEST_FLASH_SECTOR, NULL, 8);
    UART_Printf("测试NULL指针Read，预期报错，返回=%d\r\n", ret);

    ret = BSP_FLASH_WritePage(TEST_FLASH_SECTOR, NULL, 8);
    UART_Printf("测试NULL指针Write，预期报错，返回=%d\r\n", ret);

    ret = BSP_FLASH_Read(0xFFFFFFFF, read_buf, 8);
    UART_Printf("测试非法地址Read，预期报错，返回=%d\r\n", ret);

    UART_Printf(" SPI Flash 测试通过\r\n");
}


// void Test_CAN(void)   //需开启回环模式
// {
//     uint8_t ret;
//     CAN_Message_t tx_msg = {0};
//     CAN_Message_t rx_msg = {0};

//     UART_Printf("\r\n--- CAN 模块测试 ---\r\n");

//     /* 先清空接收队列残留旧报文，防止上一轮测试残留干扰 */
//     while(CAN_ReceiveMessage(&rx_msg) == BSP_TEST_OK)
//     {
//         ;
//     }

//     /* 组装发送报文 */
//     tx_msg.id = TEST_CAN_STD_ID;
//     tx_msg.dlc = 8;
//     for(uint8_t i = 0; i < 8; i++)
//     {
//         tx_msg.data[i] = i + 0x10;
//     }

//     /* 正常用例1：发送报文 */
//     UART_Printf("发送标准帧 ID=0x%03X, DLC=%d\r\n", tx_msg.id, tx_msg.dlc);
//     ret = CAN_SendMessage(&tx_msg);
//     if(ret != BSP_TEST_OK)
//     {
//         UART_Printf("CAN发送失败，错误码: %d\r\n", ret);
//         UART_Printf("当前错误状态: %d\r\n", CAN_GetErrorState());
//     }
//     else
//     {
//         UART_Printf("报文发送成功\r\n");

//         /* 轮询等待接收，最多等待100ms，不使用固定延时 */
//         uint32_t wait_cnt = 0;
//         const uint32_t max_wait = pdMS_TO_TICKS(100);
//         for(; wait_cnt < max_wait; wait_cnt++)
//         {
//             ret = CAN_ReceiveMessage(&rx_msg);
//             if(ret == BSP_TEST_OK)
//             {
//                 break;
//             }
//             vTaskDelay(pdMS_TO_TICKS(1));
//         }

//         if(ret == BSP_TEST_OK)
//         {
//             UART_Printf("接收到报文 ID=0x%03X, DLC=%d\r\n", rx_msg.id, rx_msg.dlc);
//             UART_Printf("RX DATA: %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
//                 rx_msg.data[0],rx_msg.data[1],rx_msg.data[2],rx_msg.data[3],
//                 rx_msg.data[4],rx_msg.data[5],rx_msg.data[6],rx_msg.data[7]);

//             /* 校验ID、长度、数据一致性 */
//             if((rx_msg.id == tx_msg.id) && (rx_msg.dlc == tx_msg.dlc) &&
//                (memcmp(rx_msg.data, tx_msg.data, tx_msg.dlc) == 0))
//             {
//                 UART_Printf("收发数据完全一致，CAN收发正常\r\n");
//             }
//             else
//             {
//                 UART_Printf("收发数据不一致\r\n");
//             }
//         }
//         else
//         {
//             UART_Printf("等待超时，未收到回环报文\r\n");
//             UART_Printf("当前错误状态: %d\r\n", CAN_GetErrorState());
//         }
//     }

//     /* --------异常用例：非法入参校验-------- */
//     ret = CAN_SendMessage(NULL);
//     UART_Printf("测试Send NULL指针，预期参数错误，ret=%d\r\n", ret);

//     ret = CAN_ReceiveMessage(NULL);
//     UART_Printf("测试Receive NULL指针，预期参数错误，ret=%d\r\n", ret);

//     UART_Printf(" CAN 测试完成\r\n");
// }

void Test_CAN(void)  //CAN调试器+上位机
{
    uint8_t ret_send;
    uint8_t ret_recv;
    CAN_Message_t tx_msg = {0};
    CAN_Message_t rx_msg = {0};

    UART_Printf("\r\n--- CAN 模块测试 ---\r\n");

    /* 清空队列残留旧报文 */
    while(CAN_ReceiveMessage(&rx_msg) == BSP_TEST_OK)
    {
        ;
    }

    /* 组装发送报文 */
    tx_msg.id = TEST_CAN_STD_ID;
    tx_msg.dlc = 8;
    for(uint8_t i = 0; i < 8; i++)
    {
        tx_msg.data[i] = i + 0x10;
    }

    /* 测试1：开发板发送，电脑上位机观察接收 */
    UART_Printf("\r\n[发送测试]开发板向外发送标准帧 ID=0x%03X, DLC=%d\r\n", tx_msg.id, tx_msg.dlc);
    UART_Printf("请在上位机观察是否收到：ID=0x%03X, data:10 11 12 13 14 15 16 17\r\n",tx_msg.id);

    ret_send = CAN_SendMessage(&tx_msg);
    if(ret_send != BSP_TEST_OK)
    {
        UART_Printf("CAN发送失败，错误码: %d\r\n", ret_send);
        UART_Printf("当前错误状态: %d\r\n", CAN_GetErrorState());
    }
    else
    {
        UART_Printf("报文发送成功，请确认上位机收到报文\r\n");
    }


    /* 测试2：接收测试：上位机主动下发报文给板子 */
    UART_Printf("\r\n[接收测试]请在上位机发送：ID=0x%03X DLC=8 data=10 11 12 13 14 15 16 17\r\n",TEST_CAN_STD_ID);
    UART_Printf("等待上位机下发报文，最长等待5000ms\r\n");

    uint32_t wait_cnt = 0;
    const uint32_t max_wait = pdMS_TO_TICKS(5000);
    ret_recv = BSP_TEST_FAIL;
    for(; wait_cnt < max_wait; wait_cnt++)
    {
        ret_recv = CAN_ReceiveMessage(&rx_msg);
        if(ret_recv == BSP_TEST_OK)
        {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    if(ret_recv == BSP_TEST_OK)
    {
        UART_Printf("接收到报文 ID=0x%03X, DLC=%d\r\n", rx_msg.id, rx_msg.dlc);
        UART_Printf("RX DATA: %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
            rx_msg.data[0],rx_msg.data[1],rx_msg.data[2],rx_msg.data[3],
            rx_msg.data[4],rx_msg.data[5],rx_msg.data[6],rx_msg.data[7]);

        if((rx_msg.id == tx_msg.id) && (rx_msg.dlc == tx_msg.dlc) &&
           (memcmp(rx_msg.data, tx_msg.data, tx_msg.dlc) == 0))
        {
            UART_Printf("上位机下发报文校验完全一致，CAN接收正常\r\n");
        }
        else
        {
            UART_Printf("收到报文，但ID/数据不一致\r\n");
        }
    }
    else
    {
        UART_Printf("等待超时，没有收到上位机下发报文\r\n");
        UART_Printf("当前错误状态: %d\r\n", CAN_GetErrorState());
    }

    /* --------异常用例：非法入参校验-------- */
    uint8_t ret;
    ret = CAN_SendMessage(NULL);
    UART_Printf("\r\n测试Send NULL指针，预期参数错误，ret=%d\r\n", ret);

    ret = CAN_ReceiveMessage(NULL);
    UART_Printf("测试Receive NULL指针，预期参数错误，ret=%d\r\n", ret);

    UART_Printf(" CAN 测试完成\r\n");
}
