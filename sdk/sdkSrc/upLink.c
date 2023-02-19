#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "upLink.h"
#include "osal.h"
#include "williotSdkJson.h"
#include "cJSON.h"
#include "network-api.h"
#include "logger.h"
#include "sdkConfigurations.h"
#include "mqttTopics.h"
#include "networkManager.h"
#include "sdkUtils.h"


#ifndef SIZE_OF_UP_LINK_MEMORY_POOL
#define SIZE_OF_UP_LINK_MEMORY_POOL                 (4096)
#endif
#ifndef SIZE_OF_UP_LINK_THREAD
#define SIZE_OF_UP_LINK_THREAD                      (2048)
#endif

#define SIZE_OF_ADV_PAYLOAD                         (29)
#define ADV_PACKET_PAYLOAD_OFFSET                   (2)
#define SIZE_OF_UP_LINK_QUEUE                       (5)
#define SIZE_STATIC_PAYLOAD                         (2 * SIZE_OF_ADV_PAYLOAD + 1)
#define SIZE_OF_PAYLOAD(length)                     ((length) - ADV_PACKET_PAYLOAD_OFFSET)
#define IS_FILTERED_DATA(dataBuffer,filterBuffer)   (memcmp(&((uint8_t*)(dataBuffer))[UUID_FIRST_INDEX], (filterBuffer), sizeof(filterBuffer)) == 0)
#define PAYLOAD_STRING_OFFSET_BYTE(i)               (2*(i))

#define UUID_FIRST_INDEX                            (2)
#define UUID_SECOND_INDEX                           (3)

OSAL_CREATE_POOL(s_upLinkMemPool, SIZE_OF_UP_LINK_MEMORY_POOL);

typedef struct{
    uint32_t timestamp;
    uint32_t sequenceId;
    int32_t rssi;
    char * payload;
} UpLinkMsg;

static void upLinkMsgThreadFunc();
static Queue_t s_queueOfUpLinkMsg = NULL;
static uint8_t williotUUID[] = {0,0};

#ifdef DYNAMIC_ALLOCATION_USED
OSAL_THREAD_CREATE(upLinkMsgThread, upLinkMsgThreadFunc, SIZE_OF_UP_LINK_THREAD, THREAD_PRIORITY_MEDIUM);
#else
OSAL_THREAD_CREATE_FROM_POOL(upLinkMsgThread, upLinkMsgThreadFunc, SIZE_OF_UP_LINK_THREAD, THREAD_PRIORITY_MEDIUM, &s_upLinkMemPool);
#endif
static inline void freeMsgAndStruct(void* msg, void* strc)
{
    if(msg)
    {
        OsalFreeFromMemoryPool(msg, s_upLinkMemPool);
    }
    if(strc)
    {
        OsalFreeFromMemoryPool(strc, s_upLinkMemPool);
    }
}

static UpLinkMsg* createUpLinkMsg(uint32_t timestamp,uint32_t sequenceId, int32_t rssi, void* payload)
{
    assert(payload);
    UpLinkMsg * newUpLinkMsgPtr = NULL;

    newUpLinkMsgPtr = (UpLinkMsg*)OsalMallocFromMemoryPool(sizeof(UpLinkMsg), s_upLinkMemPool);
    if(!newUpLinkMsgPtr)
    {
        return NULL;
    }

    newUpLinkMsgPtr->timestamp = timestamp;
    newUpLinkMsgPtr->sequenceId = sequenceId;
    newUpLinkMsgPtr->rssi = rssi;
    newUpLinkMsgPtr->payload = payload;

    return newUpLinkMsgPtr;
}

static inline char * createUpLinkPayload(const void* data, uint32_t length)
{
    if (!data || (length == 0))
    {
        printk("ignored illegal packet");
        return NULL;
    }
    if ((SIZE_OF_PAYLOAD(length) != SIZE_OF_ADV_PAYLOAD))
    {
        printk("ignored illegal packet length");
        return NULL;
    }

    char * payload = (char*)OsalMallocFromMemoryPool(SIZE_OF_ADV_PAYLOAD, s_upLinkMemPool);
    if(!payload)
    {
        return NULL;
    }
    memcpy(payload, ((char*)data + ADV_PACKET_PAYLOAD_OFFSET), SIZE_OF_ADV_PAYLOAD);

    return payload;
}

static void bleAdvDataProcessCallback(dev_handle dev, const void* data, uint32_t length, int32_t rssi)
{
    uint32_t timestamp = 0;
    static uint32_t s_sequenceId = 0;
    char * payload = NULL;
    UpLinkMsg * newUpLinkMsgPtr = NULL;
    SDK_STAT status = SDK_SUCCESS;

    if(IS_FILTERED_DATA(data, williotUUID))
	{
        timestamp = OsalGetTime();
        payload = createUpLinkPayload(data, length);
        if(!payload)
        {
            return;
        }

        newUpLinkMsgPtr = createUpLinkMsg(timestamp, (s_sequenceId++), rssi, payload);
        if(!newUpLinkMsgPtr)
        {
            freeMsgAndStruct(payload, NULL);
            return;
        }

        status = OsalQueueEnqueue(s_queueOfUpLinkMsg, newUpLinkMsgPtr, s_upLinkMemPool);
        if(status != SDK_SUCCESS)
        {
            freeMsgAndStruct(newUpLinkMsgPtr->payload, newUpLinkMsgPtr);
        }
	}
}

// JSON wrapping, free the string manualy
static char * createUpLinkJson(UpLinkMsg * lastUpLinkMsg, char * upLinkPayloadString)
{
    char *string = NULL;

    cJSON *upLinkJson = JsonHeaderCreate();
    if(!upLinkJson)
    {
        return NULL;
    }

    cJSON *packageTimestamp = cJSON_CreateNumber(lastUpLinkMsg->timestamp);
    JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(packageTimestamp,upLinkJson);
    cJSON_AddItemToObject(upLinkJson, "timestamp", packageTimestamp);

    cJSON *array = cJSON_CreateArray();
    JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(array,upLinkJson);

    cJSON *insideArr = cJSON_CreateObject();
    JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(insideArr,array);
    JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(insideArr,upLinkJson);

    cJSON *timestamp = cJSON_CreateNumber(lastUpLinkMsg->timestamp);
    JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(timestamp,upLinkJson);
    cJSON_AddItemToObject(insideArr, "timestamp", timestamp);

    cJSON *sequenceId = cJSON_CreateNumber(lastUpLinkMsg->sequenceId);
    JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(sequenceId,upLinkJson);
    cJSON_AddItemToObject(insideArr, "sequenceId", sequenceId);

    cJSON *rssi = cJSON_CreateNumber(lastUpLinkMsg->rssi);
    JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(rssi,upLinkJson);
    cJSON_AddItemToObject(insideArr, "rssi", rssi);

    cJSON *payload = cJSON_CreateString(upLinkPayloadString);
    JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(payload,upLinkJson);
    cJSON_AddItemToObject(insideArr, "payload", payload);

    cJSON_AddItemToArray(array, insideArr);
    cJSON_AddItemToObject(upLinkJson, "packets", array);

#ifdef DEBUG
    // SDK_STAT status = SDK_SUCCESS;
    // char* debugString =  cJSON_Print(upLinkJson);
    // assert(debugString);
    // LOG_DEBUG_INTERNAL("\n%s\n",debugString);
    // assert(status == SDK_SUCCESS);
    // FreeJsonString(debugString);
#endif

    string = cJSON_PrintUnformatted(upLinkJson);
    assert(string);

    cJSON_Delete(upLinkJson);
    return string;
}

static void convertPayloadToString(char * targetString, char * destString, uint16_t destStringSize)
{
    for(int i = 0; i < SIZE_OF_ADV_PAYLOAD; i++)
    {
        uint8_t numFromAdvPayload = targetString[i];
        sprintf(&destString[PAYLOAD_STRING_OFFSET_BYTE(i)],"%02X",numFromAdvPayload);
    }
}

static void sendLastUpLinkMsg(UpLinkMsg * lastUpLinkMsg)
{
    SDK_STAT status = SDK_SUCCESS;
    char * upLinkJson = NULL;
    static char s_lastUpLinkPayloadString[SIZE_STATIC_PAYLOAD] = {0};

    convertPayloadToString(lastUpLinkMsg->payload, s_lastUpLinkPayloadString, SIZE_STATIC_PAYLOAD);
    upLinkJson = createUpLinkJson(lastUpLinkMsg, s_lastUpLinkPayloadString);

    status = NetworkMqttMsgSend(GetMqttUpLinkTopic(), (void*) upLinkJson, strlen(upLinkJson));


    FreeJsonString(upLinkJson);
    freeMsgAndStruct(lastUpLinkMsg->payload, lastUpLinkMsg);
}

static void upLinkMsgThreadFunc()
{
    SDK_STAT status = SDK_SUCCESS;
    UpLinkMsg * lastUpLinkMsg = NULL;

    while(true)
    {
        status = OsalQueueWaitForObject(s_queueOfUpLinkMsg, 
                                        ((void **)(&(lastUpLinkMsg))),
                                        s_upLinkMemPool, OSAL_FOREVER_TIMEOUT);
        assert(status == SDK_SUCCESS);
        sendLastUpLinkMsg(lastUpLinkMsg);
    }
}

SDK_STAT UpLinkInit(dev_handle bleDev, dev_handle localDev)
{
    SDK_STAT status = SDK_SUCCESS;

    status = GetUuidToFilter((uint16_t *) williotUUID);
    RETURN_ON_FAIL(status, SDK_SUCCESS, status);

    #ifdef DYNAMIC_ALLOCATION_USED
    s_queueOfUpLinkMsg = OsalQueueCreate(SIZE_OF_UP_LINK_QUEUE);
    #else
    s_queueOfUpLinkMsg = OsalQueueCreate(SIZE_OF_UP_LINK_QUEUE, s_upLinkMemPool);
    #endif
    if(!s_queueOfUpLinkMsg)
    {
        return SDK_FAILURE;
    }

    if(bleDev)
    {
        status = RegisterReceiveCallback(bleDev, bleAdvDataProcessCallback);
        RETURN_ON_FAIL(status, SDK_SUCCESS, status);
    }

    if(localDev)
    {
        status = RegisterReceiveCallback(localDev, bleAdvDataProcessCallback);
        RETURN_ON_FAIL(status, SDK_SUCCESS, status);
    }
	
    return status;
}