#ifndef __WELCOME_LIGHT_H
#define __WELCOME_LIGHT_H

#include <stdint.h>

// 迎宾灯持续时长 ms
#define WELCOME_LIGHT_DURATION_MS     10000U

void WelcomeLight_Init(void);


void WelcomeLight_OnUnlockEvent(void);


void WelcomeLight_Process(void);


uint8_t WelcomeLight_IsRunning(void);


#endif
