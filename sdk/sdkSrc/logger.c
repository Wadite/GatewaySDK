#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "logger.h"
#include "osal.h"
#include "williotSdkJson.h"
#include "cJSON.h"
#include "network-api.h"
#include "dev-if.h"
#include "sdkConfigurations.h"
#include "sdkUtils.h"
#include "networkManager.h"
#include "mqttTopics.h"

#ifndef SIZE_OF_LOG_MEMORY_POOL
#define SIZE_OF_LOG_MEMORY_POOL         (4096)
#endif
#ifndef SIZE_OF_LOG_THREAD
#define SIZE_OF_LOG_THREAD              (2048)
#endif

#define SIZE_OF_TIME_AND_TYPE           (28)
#define SIZE_OF_MAX_LOG_MSG             (512)
#define MS_IN_SECOND					(1000)
#define MS_IN_MINUTE					(MS_IN_SECOND * 60)
#define MS_IN_HOUR						(MS_IN_MINUTE * 60)
#define MS_IN_DAY						(MS_IN_HOUR * 24)
#define LOG_TIMEOUT_TIME_MS             (600000) // 10 minutes

OSAL_CREATE_POOL(s_loggerMemPool, SIZE_OF_LOG_MEMORY_POOL);

typedef struct{
    uint32_t timestamp;
    eLogTypes logType;
    char * message;
} LogMsg;

static void logMsgThreadFunc();

#ifdef DYNAMIC_ALLOCATION_USED
OSAL_THREAD_CREATE(logMsgThread, logMsgThreadFunc, SIZE_OF_LOG_THREAD, THREAD_PRIORITY_LOW);
#else
OSAL_THREAD_CREATE_FROM_POOL(logMsgThread, logMsgThreadFunc, SIZE_OF_LOG_THREAD, THREAD_PRIORITY_LOW, s_loggerMemPool);
#endif

const char * s_logTypes[] = {
    [LOG_TYPE_DEBUG]	= "Debug",
	[LOG_TYPE_INFO]		= "Info",
	[LOG_TYPE_WARNING]	= "Warning",
	[LOG_TYPE_ERROR]	= "Error",
    [LOG_TYPE_DEBUG_INTERNAL]	= "Debug_Internal",
};

static logger_local_handle s_loggerLocalHandle = 0;
static Queue_t s_queueOfLogMsg = NULL;
static LogMsg ** s_tableOfLogMsg = NULL;
static int s_numberOfMaxLogMsg = 0;
static eLogTypes s_loggerSeverityToSend = LOG_TYPE_DEBUG;

// JSON wrapping, free the string manualy
static char * createLogJson(uint16_t indexOfLogMsg)
{
    char *string = NULL;
    size_t index = 0;
    SDK_STAT status = SDK_SUCCESS;

    cJSON *logJson = JsonHeaderCreate();
    if(!logJson)
    {
        return NULL;
    }

    cJSON *numOfLogs = cJSON_CreateNumber(indexOfLogMsg);
    JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(numOfLogs,logJson);
    cJSON_AddItemToObject(logJson, "numOfLogs", numOfLogs);

    cJSON *package = cJSON_CreateArray();
    JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(package,logJson);
    cJSON_AddItemToObject(logJson, "package", package);

    for (index = 0; index < indexOfLogMsg; ++index)
    {
        cJSON *data = cJSON_CreateObject();
        JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(data,logJson);
        cJSON_AddItemToArray(package, data);

        cJSON *timestamp = cJSON_CreateNumber(s_tableOfLogMsg[index]->timestamp);
        JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(timestamp,logJson);
        cJSON_AddItemToObject(data, "timestamp", timestamp);

        cJSON *severity = cJSON_CreateString(s_logTypes[s_tableOfLogMsg[index]->logType]);
        JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(severity,logJson);
        cJSON_AddItemToObject(data, "severity", severity);

        cJSON *Message = cJSON_CreateString(s_tableOfLogMsg[index]->message);
        JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(Message,logJson);
        cJSON_AddItemToObject(data, "Message", Message);
    }

#ifdef DEBUG
    char* debugString =  cJSON_Print(logJson);
    assert(debugString);
    status = DevLoggerSend(s_loggerLocalHandle, (void*)debugString, strlen(debugString)+1);
    assert(status == SDK_SUCCESS);
    FreeJsonString((void*)debugString);
#endif

    string = cJSON_PrintUnformatted(logJson);
    assert(string);

    cJSON_Delete(logJson);
    return string;
}

static inline void freeMsgAndStruct(void* msg, void* strc)
{
    if(msg)
    {
        OsalFreeFromMemoryPool(msg, s_loggerMemPool);
    }
    if(strc)
    {
        OsalFreeFromMemoryPool(strc, s_loggerMemPool);
    }
}

static inline int getTimeStr(char* timeStr, uint32_t timestamp)
{
    uint32_t milisecs = 0;
    uint32_t seconds = 0;
    uint32_t minutes = 0;
    uint32_t hours = 0;
    uint32_t days = 0;

	milisecs = timestamp % MS_IN_SECOND;
	seconds = timestamp % MS_IN_MINUTE / MS_IN_SECOND;
	minutes = timestamp % MS_IN_HOUR / MS_IN_MINUTE;
	hours = timestamp % MS_IN_DAY / MS_IN_HOUR;
	days = timestamp / MS_IN_DAY;

    return sprintf(timeStr ,"[%03u:%02u:%02u:%02u:%03u]", days, hours, minutes, seconds, milisecs);
}

static void localLog(LogMsg * logMsg)
{
    SDK_STAT status;
    int offset = 0;
    static char logBuff[SIZE_OF_TIME_AND_TYPE] = {0};

    offset += getTimeStr(logBuff, logMsg->timestamp);
    offset += sprintf((logBuff + offset)," %-7s :",s_logTypes[logMsg->logType]);

    status = DevLoggerSend(s_loggerLocalHandle, (void*)logBuff, strlen(logBuff)+1);
    assert(status == SDK_SUCCESS);
    
    status = DevLoggerSend(s_loggerLocalHandle, (void*)logMsg->message, strlen(logMsg->message)+1);
    assert(status == SDK_SUCCESS);
}

static char * msgArgProcess(const char * message, va_list args)
{
    char tempLogMsg = 0;
    char * newMsgPtr = NULL;
    int sizeOfMsg = 0;

    sizeOfMsg = vsnprintf(&tempLogMsg, sizeof(tempLogMsg), message, args);

    newMsgPtr = (char*)OsalMallocFromMemoryPool(sizeOfMsg + 1, s_loggerMemPool);
    if(!newMsgPtr)
    {
        return NULL;
    }

    vsnprintf(newMsgPtr, sizeOfMsg, message, args);

    return newMsgPtr;
}

static LogMsg * buildLogMsgStruct(uint32_t timestamp, eLogTypes logType, char* message)
{
    LogMsg * newLogMsgPtr = NULL;

    newLogMsgPtr = (LogMsg*)OsalMallocFromMemoryPool(sizeof(LogMsg), s_loggerMemPool);
    if(!newLogMsgPtr)
    {
        return NULL;
    }

    newLogMsgPtr->timestamp = timestamp;
    newLogMsgPtr->logType = logType;
    newLogMsgPtr->message = message;

    return newLogMsgPtr;
}

static void sendCurrentTable(uint16_t tableIndex)
{
    SDK_STAT status = SDK_SUCCESS;
    char * logJSON = NULL;

    if(tableIndex == 0)
    {
        return;
    }

    logJSON = createLogJson(tableIndex);
    status = NetworkMqttMsgSend(GetMqttStatusTopic(), (void*) logJSON, strlen(logJSON)+1);

    FreeJsonString((void*)logJSON);

    for(uint16_t i = 0; i < tableIndex; i++)
    {
#ifndef DEBUG
        localLog(s_tableOfLogMsg[i]);
#endif
        // Memory free
        freeMsgAndStruct(s_tableOfLogMsg[i]->message, s_tableOfLogMsg[i]);
    }
}

static void logMsgThreadFunc()
{
    SDK_STAT status = SDK_SUCCESS;
    uint16_t indexOfLogMsg = 0;
    while(true)
    {
        status = OsalQueueWaitForObject(s_queueOfLogMsg, 
                                        ((void **)(&(s_tableOfLogMsg[indexOfLogMsg]))), 
                                        s_loggerMemPool, LOG_TIMEOUT_TIME_MS);
        if(status == SDK_SUCCESS)
        {
            indexOfLogMsg++;
#ifdef DEBUG
            uint16_t currentIndex = indexOfLogMsg - 1;
            localLog(s_tableOfLogMsg[currentIndex]);
            if(LOG_TYPE_DEBUG_INTERNAL == s_tableOfLogMsg[currentIndex]->logType)
            {
                freeMsgAndStruct(s_tableOfLogMsg[currentIndex]->message, s_tableOfLogMsg[currentIndex]);
                indexOfLogMsg--;
            }
#endif
        }

        if(indexOfLogMsg == s_numberOfMaxLogMsg || (status == SDK_TIMEOUT))
        {
            sendCurrentTable(indexOfLogMsg);
            indexOfLogMsg = 0;
        }
    }
}

SDK_STAT LoggerInit()
{
    SDK_STAT status = SDK_SUCCESS;

    status = GetLoggerNumberOfLogs(&s_numberOfMaxLogMsg);
    RETURN_ON_FAIL(status, SDK_SUCCESS, status);

    s_tableOfLogMsg = (LogMsg **)OsalMallocFromMemoryPool((s_numberOfMaxLogMsg * sizeof(LogMsg *)), s_loggerMemPool);
    if(!s_tableOfLogMsg)
    {
        return SDK_FAILURE;
    }
    s_loggerLocalHandle = DevLoggerInit();
    if(!s_loggerLocalHandle)
    {
        OsalFreeFromMemoryPool(s_tableOfLogMsg, s_loggerMemPool);
        return SDK_FAILURE;
    }

    #ifdef DYNAMIC_ALLOCATION_USED
    s_queueOfLogMsg = OsalQueueCreate(s_numberOfMaxLogMsg);
    #else
    s_queueOfLogMsg = OsalQueueCreate(s_numberOfMaxLogMsg, s_loggerMemPool);
    #endif
    
    if(!s_queueOfLogMsg)
    {
        OsalFreeFromMemoryPool(s_tableOfLogMsg, s_loggerMemPool);
        return SDK_FAILURE;
    }

    status = GetLoggerSeverity(&s_loggerSeverityToSend);

    return status;
  
}

static bool shouldSendLog(eLogTypes logType)
{
    return logType >= s_loggerSeverityToSend;
}

void LogSend(eLogTypes logType, const char* message, ...)
{   
    assert(message);
    assert(logType < LOG_TYPE_NUM);

    uint32_t timestamp = 0;
    char * newMsgPtr = NULL;
    LogMsg * newLogMsgPtr = NULL;
    SDK_STAT status = SDK_SUCCESS;

    if(!shouldSendLog(logType))
    {
        return;
    }

    va_list args;   
    va_start(args, message);

    timestamp = OsalGetTime();
    newMsgPtr = msgArgProcess(message, args);
    va_end(args);
    if(!newMsgPtr)
    {
        return;
    }

    newLogMsgPtr = buildLogMsgStruct(timestamp, logType, newMsgPtr);
    if(!newLogMsgPtr)
    {
        freeMsgAndStruct(newMsgPtr, NULL);
        return;
    }
    
    status = OsalQueueEnqueue(s_queueOfLogMsg, newLogMsgPtr, s_loggerMemPool);
    if(status != SDK_SUCCESS)
    {
        freeMsgAndStruct(newMsgPtr, newLogMsgPtr);
        return;
    }
}
