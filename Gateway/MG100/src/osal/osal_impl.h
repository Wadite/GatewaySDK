#ifndef _OSAL_IMPL_H_
#define _OSAL_IMPL_H_

#include <zephyr/kernel.h>

#include "osal_defines.h"

#define DYNAMIC_ALLOCATION_USED 1
#define USER_CREATE_POOL(name, size) K_HEAP_DEFINE(name##_internal, size); static MemoryPool_t name = &name##_internal
#define CONVERTED_PRIORITY(priority)   (THREAD_PRIORITY_NUM - (priority))

#define USER_THREAD_CREATE(name, func, stackSize, priority) \
        K_THREAD_DEFINE(name, stackSize, OsalCallbackWrapper, func, NULL, NULL , CONVERTED_PRIORITY(priority), 0, 0)		

void OsalCallbackWrapper(void * func1, void * func2, void * func3);

#endif //_OSAL_IMPL_H_