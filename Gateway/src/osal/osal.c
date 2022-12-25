#include "osal.h"

#include <zephyr/kernel.h>
#include <string.h>

#define THREADS_EVENT_BITS      (1)

struct osalQueueNode {
	void * fifoReserved;
	void * dataPtr;
};

static struct k_event s_wakeThreadsEvent;

static k_timeout_t convertTimeout(uint32_t timeout)
{
    if(timeout == OSAL_FOREVER_TIMEOUT)
    {
        return K_FOREVER;
    }
    else if(timeout == OSAL_NO_TIMEOUT)
    {
        return K_NO_WAIT;
    }
    else
    {
        return K_MSEC(timeout);
    }
}

void OsalCallbackWrapper(void * func1, void * func2, void * func3)
{
    __ASSERT(func1,"NULL pointer was passed");

    int waitEventState = k_event_wait(&s_wakeThreadsEvent, THREADS_EVENT_BITS, true, K_FOREVER);
	__ASSERT((waitEventState != 0),"k_event_wait timeout");

    ThreadFunc funPtr= func1;
    funPtr(NULL);
}

int OsalConvertPriority(ThreadPriority requestedPriority)
{
    int tempPriority = 0;

    switch(requestedPriority)
    {
        case THREAD_PRIORITY_LOW:
            tempPriority = K_LOWEST_APPLICATION_THREAD_PRIO;
            break;

        case THREAD_PRIORITY_MEDIUM:
            tempPriority = 0;
            break;

        case THREAD_PRIORITY_HIGH:
            tempPriority = K_HIGHEST_APPLICATION_THREAD_PRIO;
            break;
    }
    return tempPriority;
}

uint32_t OsalGetTime()
{
    return k_uptime_get_32();
}

Mutex_t OsalMutexCreate()
{
    int error = 0;
    void * tempMutex = OsalMalloc(sizeof(struct k_mutex));

    if(!tempMutex)
    {
        return NULL;
    }

    error = k_mutex_init((struct k_mutex *)tempMutex);   

    if(error)
    {
        OsalFree(tempMutex);
        return NULL;
    }

    return tempMutex;
}

SDK_STAT OsalMutexLock(Mutex_t mutex, uint32_t timeout)
{
    k_timeout_t timeOutMs;
    int error;
    SDK_STAT sdkStat = SDK_SUCCESS;

    if(!mutex)
    {
        return SDK_INVALID_PARAMS;
    }

    timeOutMs = (!timeout) ? K_FOREVER : K_MSEC(timeout); 
    error = k_mutex_lock((struct k_mutex *)mutex, timeOutMs);

    switch(error)
    {
        case 0:
            sdkStat = SDK_SUCCESS;
            break;
        
        case -EBUSY:
            sdkStat = SDK_BUSY;
            break;
        
        case -EAGAIN:
            sdkStat = SDK_TIMEOUT;
            break;
        
        default:
            __ASSERT(true,"Unexpected error occurred");
            break;
    }

    return sdkStat;
}

SDK_STAT OsalMutexUnlock(Mutex_t mutex)
{
    int error = 0;
    SDK_STAT sdkStat = SDK_SUCCESS;

    if(!mutex)
    {
        return SDK_INVALID_PARAMS;
    }

    error = k_mutex_unlock((struct k_mutex *)mutex);

    switch(error)
    {
        case 0:
            sdkStat = SDK_SUCCESS;
            break;
        
        case -EPERM:
            sdkStat = SDK_MUTEX_NOT_OWNED;
            break;
        
        case -EINVAL:
            sdkStat = SDK_INVALID_STATE;
            break;
        
        default:
            __ASSERT(true,"Unexpected error occurred");
            break;
    }

    return sdkStat;
}

Queue_t OsalQueueCreate(uint32_t sizeOfQueue)
{
    void * tempFifo = OsalMalloc(sizeof(struct k_fifo));

    if(!tempFifo)
    {
        return NULL;
    }

    k_fifo_init((struct k_fifo *)tempFifo);
    return tempFifo;
}

SDK_STAT OsalQueueEnqueue(Queue_t queue, DataItem_t item, MemoryPool_t pool)
{
    if(!queue || !item || !pool)
    {
        return SDK_INVALID_PARAMS;
    }

    struct osalQueueNode * tempNode = OsalCallocFromMemoryPool(sizeof(struct osalQueueNode), pool);

    if(!tempNode)
    {
        return SDK_FAILURE;
    }

    tempNode->dataPtr = item;

    k_fifo_put((struct k_fifo *)queue, (void *)tempNode);
    return SDK_SUCCESS;
}

SDK_STAT OsalQueueWaitForObject(Queue_t queue, DataItem_t* item, MemoryPool_t pool, uint32_t timeout)
{
    if(!queue || !item || !pool)
    {
        return SDK_INVALID_PARAMS;
    }
    struct osalQueueNode * tempNode;

    tempNode = (struct osalQueueNode*)(k_fifo_get((struct k_fifo *)queue, convertTimeout(timeout)));
    if(!tempNode)
    {
        return SDK_TIMEOUT;
    }
    *item = tempNode->dataPtr;
    OsalFreeFromMemoryPool(tempNode, pool);
    return (*item) ? SDK_SUCCESS : SDK_FAILURE;
}

bool OsalQueueIsEmpty(Queue_t queue)
{
    if(!queue)
    {
        return false;
    }    
    int isEmpty = k_fifo_is_empty((struct k_fifo *)queue);
    return !isEmpty ? false : true; 
}

Timer_t OsalTimerCreate(TimerFunc func)
{
    if(!func)
    {
        return NULL;
    }

    void * tempTimer = OsalMalloc(sizeof(struct k_timer));

    if(!tempTimer)
    {
        return NULL;
    }
    
    k_timer_init((struct k_timer *)tempTimer, (k_timer_expiry_t)func, NULL);
    return tempTimer;
}

SDK_STAT OsalTimerStart(Timer_t timer, uint32_t period)
{
    if(!timer)
    {
        return SDK_INVALID_PARAMS;
    }

    k_timer_start((struct k_timer *)timer, K_MSEC(period), K_MSEC(period));
    return SDK_SUCCESS;     
}

SDK_STAT OsalTimerStop(Timer_t timer)
{
    if(!timer)
    {
        return SDK_INVALID_PARAMS;
    }

    k_timer_stop((struct k_timer *)timer);
    return SDK_SUCCESS;
}

void OsalSleep(uint32_t sleepTime)
{
    k_sleep(K_MSEC(sleepTime));
}

void* OsalMalloc(uint32_t size)
{
    void* memPtr;

    if(!size)
    {
        return NULL;
    }

    memPtr = k_malloc(size);
    return memPtr;
}

void* OsalMallocFromMemoryPool(uint32_t size, MemoryPool_t pool)
{
    void* memPtr;

    if(!pool || !size)
    {
        return NULL;
    }
    
    memPtr = k_heap_alloc((struct k_heap *)pool, size, K_NO_WAIT);
    return memPtr;
}

void* OsalCalloc(uint32_t size)
{
    void* memPtr;

    if(!size)
    {
        return NULL;
    }

    memPtr = k_malloc(size);

    if (memPtr) 
    {
        memset(memPtr, 0, size);
    }

    return memPtr;
}

void* OsalCallocFromMemoryPool(uint32_t size, MemoryPool_t pool)
{
    void* memPtr;

    if(!pool || !size)
    {
        return NULL;
    }

    memPtr = k_heap_alloc((struct k_heap *)pool, size, K_NO_WAIT);

    if (memPtr) 
    {
        memset(memPtr, 0, size);
    }

    return memPtr;
}

void OsalFree(void* ptr)
{
    if(!ptr)
    {
        return;
    }

    k_free(ptr);
}

void OsalFreeFromMemoryPool(void* ptr, MemoryPool_t pool)
{
    if(!pool || !ptr)
    {
        return;
    }
    // printk("freeing %p from %p \n", ptr, pool);
    k_heap_free((struct k_heap *)pool, ptr);
}

void OsalInit()
{
    k_event_init(&s_wakeThreadsEvent);
}

void OsalStart()
{
    k_event_post(&s_wakeThreadsEvent, THREADS_EVENT_BITS);
}