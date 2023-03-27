#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "downLink.h"
#include "osal.h"
#include "network-api.h"
#include "cJSON.h"
#include "sdkConfigurations.h"
#include "mqttTopics.h"
#include "williotSdkJson.h"
#include "sdkUtils.h"


#ifndef SIZE_OF_DOWN_LINK_THREAD
#define SIZE_OF_DOWN_LINK_THREAD            (2048) 
#endif
#ifndef SIZE_OF_DOWN_LINK_MEMORY_POOL
#define SIZE_OF_DOWN_LINK_MEMORY_POOL       (2048)
#endif

#define SIZE_OF_DOWN_LINK_QUEUE             (5)
#define DEFAULT_ADV_DURATION                (100)
#define DEFAULT_ADV_INTERVAL                (8)
#define JSON_KEY_DURATION                   "txMaxDurationMs"
#define JSON_KEY_RETRIES                    "txMaxRetries"
#define JSON_KEY_PACKET                     "txPacket"
#define ACTION_KEY                          "action"
#define API_VERSION_KEY                     "apiVersion"
#define MAX_DATA_SIZE                       (31)
#define NUMBER_OF_CHARS_TO_READ             (2)
#define HEXADECIMAL_BASE                    (16)

#define IS_JSON(jsonPtr)                    (*(jsonPtr) == '{')

OSAL_CREATE_POOL(s_downLinkMemPool, SIZE_OF_DOWN_LINK_MEMORY_POOL);

typedef struct{
    char * payload;
    int sizeOfPayload;
} DownLinkMsg;

static void downLinkMsgThreadFunc();
static Queue_t s_queueOfDownLinkMsg = NULL;
static dev_handle s_bleDevHandle = NULL;
static dev_handle s_localDevHandle = NULL;

#ifdef DYNAMIC_ALLOCATION_USED
OSAL_THREAD_CREATE(downLinkMsgThread, downLinkMsgThreadFunc, SIZE_OF_DOWN_LINK_THREAD, THREAD_PRIORITY_HIGH);
#else
OSAL_THREAD_CREATE_FROM_POOL(downLinkMsgThread, downLinkMsgThreadFunc, SIZE_OF_DOWN_LINK_THREAD, THREAD_PRIORITY_HIGH, s_downLinkMemPool);
#endif

static void freeDownLinkMsg(DownLinkMsg * downLinkMsgPtr)
{
    if(downLinkMsgPtr)
    {
        if(downLinkMsgPtr->payload)
        {
            OsalFreeFromMemoryPool(downLinkMsgPtr->payload, s_downLinkMemPool);
        }
        OsalFreeFromMemoryPool(downLinkMsgPtr, s_downLinkMemPool);
    }
}

static void netReceiveMQTTPacketCallback(void * data, uint32_t length)
{
    SDK_STAT status = SDK_SUCCESS;
    char * payload = NULL;
    DownLinkMsg * newDownLinkMsgPtr = NULL;

    assert(data && length);
    
    payload = OsalMallocFromMemoryPool(length, s_downLinkMemPool);
    if(!payload)
    {
        return;
    }
    memcpy(payload,data,length);
    
    newDownLinkMsgPtr = (DownLinkMsg*)OsalMallocFromMemoryPool(sizeof(DownLinkMsg), s_downLinkMemPool);
    newDownLinkMsgPtr->payload = payload;
    newDownLinkMsgPtr->sizeOfPayload = length;

    status = OsalQueueEnqueue(s_queueOfDownLinkMsg, newDownLinkMsgPtr, s_downLinkMemPool);
    if(status != SDK_SUCCESS)
    {
        freeDownLinkMsg(newDownLinkMsgPtr);
    }
}

static void advJson(cJSON * advJson)
{
    SDK_STAT status = SDK_SUCCESS;
    uint32_t advDuration = DEFAULT_ADV_DURATION;
    uint32_t advInterval = DEFAULT_ADV_INTERVAL;
    char* advData = NULL;
    cJSON * jsonNode = NULL;

    jsonNode = cJSON_GetObjectItem(advJson, JSON_KEY_DURATION);
    if(jsonNode)
    {
        advDuration = (uint32_t)cJSON_GetNumberValue(jsonNode);
    }

    jsonNode = cJSON_GetObjectItem(advJson, JSON_KEY_RETRIES);
    if(jsonNode)
    {
        uint32_t retries = (uint32_t)cJSON_GetNumberValue(jsonNode); 
        advInterval = advDuration / retries;
    }

    jsonNode = cJSON_GetObjectItem(advJson, JSON_KEY_PACKET);
    if(jsonNode)
    {
        advData = cJSON_GetStringValue(jsonNode);
        uint8_t rawData[MAX_DATA_SIZE] = {0};
        char strToConv[NUMBER_OF_CHARS_TO_READ + 1] = {0};
        int sizeOfStr = (strlen(advData));

        for(int i = 0; i < sizeOfStr; i += NUMBER_OF_CHARS_TO_READ)
        {
            memcpy(strToConv, advData + i, NUMBER_OF_CHARS_TO_READ);
            rawData[i/NUMBER_OF_CHARS_TO_READ] = (uint8_t)strtol(strToConv, NULL, HEXADECIMAL_BASE);
        }

        if(s_bleDevHandle)
        {
            status = DevSendPacket(s_bleDevHandle, advDuration, advInterval, rawData, MAX_DATA_SIZE);
            assert(status == SDK_SUCCESS);
        }
        if(s_localDevHandle)
        {
            status = DevSendPacket(s_localDevHandle, advDuration, advInterval, rawData, MAX_DATA_SIZE);
            assert(status == SDK_SUCCESS);           
        }
    }
}

static void createDownlinkJson(const char* ptr)
{
    SDK_STAT status = SDK_SUCCESS;
    cJSON * downLinkJson = cJSON_Parse(ptr);

    cJSON * actionJson = cJSON_GetObjectItem(downLinkJson, ACTION_KEY);
    if(actionJson)
    {

#ifdef DEBUG
        LOG_DEBUG_INTERNAL("\nDownlink message : \n%s\n", ptr);
#endif

        advJson(downLinkJson);
        cJSON_Delete(downLinkJson);
        return;
    }

    cJSON * apiVersionJson = cJSON_GetObjectItem(downLinkJson, API_VERSION_KEY);
    if(apiVersionJson)
    {
        status = SetConfiguration(downLinkJson);
        assert(status == SDK_SUCCESS);
        cJSON_Delete(downLinkJson);
        OsalSystemReset();
        return;
    }

    cJSON_Delete(downLinkJson);
}

static void downLinkProcess(DownLinkMsg * rawDownLink)
{
    char * mqttPayload = rawDownLink->payload;

#ifdef DEBUG
    if(mqttPayload)
    {
        printk("downlink mqtt pkt:\n|%s|\n", mqttPayload); 
    }
#endif

    if(mqttPayload && IS_JSON(mqttPayload))
    {
        createDownlinkJson(mqttPayload);
    }

    freeDownLinkMsg(rawDownLink);
}

static void downLinkMsgThreadFunc()
{
    SDK_STAT status = SDK_SUCCESS;
    DownLinkMsg * rawDownLinkMsg = NULL;

    while(true)
    {
        status = OsalQueueWaitForObject(s_queueOfDownLinkMsg, 
                                        ((void **)(&(rawDownLinkMsg))),
                                        s_downLinkMemPool, OSAL_FOREVER_TIMEOUT);
        assert(status == SDK_SUCCESS);

        downLinkProcess(rawDownLinkMsg);
    }
}

SDK_STAT DownLinkInit(dev_handle bleDev, dev_handle localDev)
{
    SDK_STAT status = SDK_SUCCESS;
    
    #ifdef DYNAMIC_ALLOCATION_USED
    s_queueOfDownLinkMsg = OsalQueueCreate(SIZE_OF_DOWN_LINK_QUEUE);
    #else
    s_queueOfDownLinkMsg = OsalQueueCreate(SIZE_OF_DOWN_LINK_QUEUE, s_downLinkMemPool);
    #endif

    if(!s_queueOfDownLinkMsg)
    {
        return SDK_FAILURE;
    }

    status = RegisterNetReceiveMQTTPacketCallback(netReceiveMQTTPacketCallback);
    RETURN_ON_FAIL(status, SDK_SUCCESS, status);

    if(bleDev)
    {
        s_bleDevHandle = bleDev;
    }

    if(localDev)
    {

        s_localDevHandle = localDev;
    }
    
    return status;
}
