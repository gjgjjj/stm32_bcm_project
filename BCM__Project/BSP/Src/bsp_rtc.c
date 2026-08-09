#include "bsp_rtc.h"

extern RTC_HandleTypeDef hrtc;   

uint32_t BSP_RTC_GetTimestamp(void)
{
    uint16_t high1;
    uint16_t high2;
    uint16_t low;

    HAL_RTC_WaitForSynchro(&hrtc);   //等待 RTC 寄存器和 RTC 内核时钟同步，保证读到最新寄存器值

    do
    {
        high1 = RTC->CNTH;    //第一次读高16位
        low   = RTC->CNTL;    //读低16位
        high2 = RTC->CNTH;    //第二次读高16位

    }while(high1 != high2);   //两次高字节不一样，代表读取中途计数器进位了，重新读

    return ((uint32_t)high1 << 16U) | low;   //拼接成32位返回
}


