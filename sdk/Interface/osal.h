#ifndef _OSAL_H_
#define _OSAL_H_

#include <stdint.h>
#include <stdbool.h>
#include "sdk_common.h"
/**
@brief this h file is for defines that are used through the osal 
    dynamic allocation is used add a define DYNAMIC_ALLOCATION_USED in this file
 */
#include "osal/osal_impl.h"

#define OSAL_FOREVER_TIMEOUT   (UINT32_MAX)
#define OSAL_NO_TIMEOUT        (0)

typedef void* Mutex_t;
typedef void* Queue_t;
typedef void* Timer_t;
typedef void* Thread_t;
typedef void* MemoryPool_t;

typedef void* DataItem_t;
typedef void (*TimerFunc)(void* arg);
typedef void (*ThreadFunc)(void* arg);

/**
 * @brief Init any property that the OS should need.
 * Needs to be called before Any Other Osal Fucntion is called
 */
void OsalInit();

/**
* @brief get system's time
*
* @return Time passed since OS started in ms
 */
uint32_t OsalGetTime();

/**
 * @brief Creates a mutex
 * 
 * @param pool if dynamic allocation is not used a pool should be sent to get the mutex from
 * @return NULL upon failure, mutex upon success
 */
#ifdef DYNAMIC_ALLOCATION_USED
Mutex_t OsalMutexCreate();
#else
Mutex_t OsalMutexCreate(MemoryPool_t pool);
#endif
/**
 * @brief Locks the mutex 
 * 
 * @param mutex
 * @param timeout - amount of time to lock, 0 means forever(translation should be added per implementation)
 * @return SDK_SUCCESS upon success 
 * @return SDK_INVALID_PARAMS if mutex is NULL, 
 * @return SDK_TIMEOUT if timeout happend,
 */
SDK_STAT OsalMutexLock(Mutex_t mutex, uint32_t timeout);

/**
 * @brief Unlocks the mutex
 * 
 * @param mutex
 * @return SDK_SUCCESS upon success 
 * @return SDK_INVALID_PARAMS if mutex is NULL,
 * @return SDK_INVALID_STATE if mutex isn't locked
 * @return SDK_MUTEX_NOT_OWNED if mutex wan't locked by this thread
 */
SDK_STAT OsalMutexUnlock(Mutex_t mutex);

/**
 * @brief Creates queue of given size
 * 
 * @param sizeOfQueue -he size of the created queue
 * @param pool if dynamic allocation is not used a pool should be sent to get the queue from
 * @return NULL if failed to create the queue, queue otherwise
 */
#ifdef DYNAMIC_ALLOCATION_USED
Queue_t OsalQueueCreate(uint32_t sizeOfQueue);
#else
Queue_t OsalQueueCreate(uint32_t sizeOfQueue, MemoryPool_t pool);
#endif
/**
 * @brief Enqueue an item to the queue
 * 
 * @param queue 
 * @param item [in] item to enqueue 
 * @param pool if needed memory pool to allocate queue object
 * @return SDK_SUCCESS upon success
 * @return SDK_INVALID_PARAMS upon receiving NULL
 * @return SDK_INVALID_STATE if full
 */
SDK_STAT OsalQueueEnqueue(Queue_t queue, DataItem_t item, MemoryPool_t pool);

/**
 * @brief Waits for an item to dequeue from the queue and the dequeue it
 * This is blocking
 * @param queue 
 * @param item [out] where to put the item from the queue
 * @param pool if memory pool was used to allocate queue object
 * @param timeout how much time wait for object in ms, possible also OSAL_FOREVER_TIMEOUT/OSAL_NO_TIMEOUT
 * 
 * @return SDK_SUCCESS upon success
 * @return SDK_INVALID_PARAMS upon receiving NULL
 * @return SDK_FAILURE allocation fail
 */
SDK_STAT OsalQueueWaitForObject(Queue_t queue, DataItem_t* item, MemoryPool_t pool, uint32_t timeout);

/**
 * @brief Chceks if the queue is empty or not
 * 
 * @param queue 
 * @return true if empty
 * @return false if not empty
 */
bool OsalQueueIsEmpty(Queue_t queue);

/**
 * @brief Creates a timer with given callback
 * 
 * @param func Callback function for the timer 
 * @param pool if dynamic allocation is not used a pool should be sent to get the tiemr from
 * @return NULL if fail or timer on success
 */
#ifdef DYNAMIC_ALLOCATION_USED
Timer_t OsalTimerCreate(TimerFunc func);
#else
Timer_t OsalTimerCreate(TimerFunc func, MemoryPool_t pool);
#endif
/**
 * @brief starts the timer to work every period time continuously
 * 
 * @param timer 
 * @param period the period of the timer in milliseconds 
 * @return SDK_SUCCESS upon success
 * @return SDK_INVALID_PARAMS upon receiving NULL or period is 0
 */
SDK_STAT OsalTimerStart(Timer_t timer, uint32_t period);

/**
 * @brief Stops the timer
 * 
 * @param timer 
 * @return SDK_SUCCESS upon success
 * @return SDK_INVALID_PARAMS upon receiving NULL 
 */
SDK_STAT OsalTimerStop(Timer_t timer);

/**
 * @brief Creates a thread 
 * USER_THREAD_CREATE should be define in "osal_impl.h"
 * @param name name of thread
 * @param func thread function of type ThreadFunc_t
 * @param stackSize given stackSize to give to the thread
 * @param priority the priority of the thread
 * @param pool if dynamic allocation is not used a pool should be sent to get the thread from
 * @return NULL upon fail, thread upon success
 */
#ifdef DYNAMIC_ALLOCATION_USED
#define OSAL_THREAD_CREATE(name, func, stackSize, priority) USER_THREAD_CREATE(name, func, stackSize, priority)
#else
#define OSAL_THREAD_CREATE(name, func, stackSize, priority, pool) USER_THREAD_CREATE(name, func, stackSize, priority, pool)
#endif
/**
 * @brief sleeps given time
 * 
 * @param sleepTime in milliseconds
 */
void OsalSleep(uint32_t sleepTime);

/**
 * @brief allocates memory of given size
 * 
 * @param size 
 * @return NULL if fails, pointer to the malloced on success
 */
void* OsalMalloc(uint32_t size);

/**
 * @brief allocates memory of given size
 * 
 * @param size 
 * @param pool if dynamic allocation is not used a pool to malloc from
 * @return NULL if fails, pointer to the malloced on success
 */
void* OsalMallocFromMemoryPool(uint32_t size, MemoryPool_t pool);

/**
 * @brief allocates memory of given size and zeros it 
 * 
 * @param size 
 * @return NULL if fails, pointer to the calloced on success
 */
void* OsalCalloc(uint32_t size);

/**
 * @brief allocates memory of given size and zeros it 
 * 
 * @param size 
 * @param pool if dynamic allocation is not used a pool to calloc from
 * @return NULL if fails, pointer to the calloced on success
 */
void* OsalCallocFromMemoryPool(uint32_t size, MemoryPool_t pool);

/**
 * @brief frees malloced pointer
 * 
 * @param ptr 
 */
void OsalFree(void* ptr);

/**
 * @brief frees malloced pointer
 * 
 * @param ptr 
 * @param pool if dynamic allocation is not used a pool should be sent to return the pool to
 */
void OsalFreeFromMemoryPool(void* ptr, MemoryPool_t pool);


/**
 * @brief Starts the osal, if anything needs to be done for the main task to do
 * For example:
 * FreeRTOS - kernelStart
 */
void OsalStart();

/**
 * @brief Create A Memory pool that is used for the non dynamic Creation
 * USER_CREATE_POOL should be implemented in "osal_impl.h"
 * 
 * @param name - name of the pool
 * @param size - size of the pool
 * @return MemoryPool_t on success or NULL on error
 */
#define OSAL_CREATE_POOL(name, size) USER_CREATE_POOL(name, size)

#endif //_OSAL_H_