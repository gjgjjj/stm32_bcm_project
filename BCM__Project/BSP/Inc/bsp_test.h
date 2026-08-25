#ifndef __BSP_TEST_H
#define __BSP_TEST_H

#include "cmsis_os.h"

#define ENABLE_BSP_TEST_CODE    0U

#if ENABLE_BSP_TEST_CODE

void BSP_UnitTestTask(void *argument);

#endif

#endif