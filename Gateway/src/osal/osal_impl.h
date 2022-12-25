#ifndef _OSAL_IMPL_H_
#define _OSAL_IMPL_H_

#include <zephyr/kernel.h>

#define DYNAMIC_ALLOCATION_USED 1

#define USER_CREATE_POOL(name, size) K_HEAP_DEFINE(name, size)

// Three state priority convert (0->14, 1->0, 2->-16)
#define CONVERTED_PRIORITY(priority)   (K_LOWEST_APPLICATION_THREAD_PRIO - (priority)* \
                                        (K_LOWEST_APPLICATION_THREAD_PRIO - 1 + (priority)))

#define USER_THREAD_CREATE(name, func, stackSize, priority) \
        K_THREAD_DEFINE(name, stackSize, OsalCallbackWrapper, func, NULL, NULL , CONVERTED_PRIORITY(priority), 0, 0)		

typedef enum {
    THREAD_PRIORITY_LOW,
    THREAD_PRIORITY_MEDIUM,
    THREAD_PRIORITY_HIGH
} ThreadPriority;

void OsalCallbackWrapper(void * func1, void * func2, void * func3);
int OsalConvertPriority(ThreadPriority requestedPriority);

#endif //_OSAL_IMPL_H_