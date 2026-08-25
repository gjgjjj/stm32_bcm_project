#ifndef __TEST_H
#define __TEST_H

#define ENABLE_TEST_CODE    0U

#if ENABLE_TEST_CODE

void Test_Osal_CreateTask(void);

void Test_SignalFilter_CreateTask(void);

void Test_FaultMgr_CreateTask(void);

void Test_CanProtocol_CreateTask(void);

void Test_CanRx_Link_CreateTask(void);

void Test_KeyMgr_CreateTask(void);

void Test_StorageMgr_CreateTask(void);

void Test_ExecutorDiag_CreateTask(void);

#endif

#endif