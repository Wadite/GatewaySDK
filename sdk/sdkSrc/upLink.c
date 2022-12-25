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

#define SIZE_OF_ADV_PAYLOAD                         (29)
#define ADV_PACKET_PAYLOAD_OFFSET                   (2)
#define SIZE_OF_UP_LINK_QUEUE                       (5)
#define SIZE_OF_UP_LINK_MEMORY_POOL                 (4096)
#define SIZE_OF_UP_LINK_THREAD                      (2048)
#define SIZE_STATIC_PAYLOAD                         (2 * SIZE_OF_ADV_PAYLOAD + 1)
#define SIZE_OF_PAYLOAD(length)                     ((length) - ADV_PACKET_PAYLOAD_OFFSET)
#define IS_FILTERED_DATA(dataBuffer,filterBuffer)   (memcmp(&((uint8_t*)(dataBuffer))[UUID_FIRST_INDEX], (filterBuffer), sizeof(filterBuffer)) == 0)
#define PAYLOAD_STRING_OFFSET_BYTE(i)               (2*(i))

#define UUID_FIRST_INDEX                            (2)
#define UUID_SECOND_INDEX                           (3)
#define WILLIOT_UUID_FIRST_BYTE                     (0xFD)
#define WILLIOT_UUID_SECOND_BYTE                    (0xAF)

OSAL_CREATE_POOL(s_upLinkMemPool, SIZE_OF_UP_LINK_MEMORY_POOL);

typedef struct{
    uint32_t timestamp;
    uint32_t sequenceId;
    int32_t rssi;
    char * payload;
} UpLinkMsg;

static void upLinkMsgThreadFunc();
static Queue_t s_queueOfUpLinkMsg = NULL;
static uint8_t williotUUID[] = {0xFD,0xAF};

OSAL_THREAD_CREATE(upLinkMsgThread, upLinkMsgThreadFunc, SIZE_OF_UP_LINK_THREAD, THREAD_PRIORITY_MEDIUM);

static inline void freeMsgAndStruct(void* msg, void* strc)
{
    if(msg)
    {
        OsalFreeFromMemoryPool(msg, &s_upLinkMemPool);
    }
    if(strc)
    {
        OsalFreeFromMemoryPool(strc, &s_upLinkMemPool);
    }
}

static UpLinkMsg* createUpLinkMsg(uint32_t timestamp,uint32_t sequenceId, int32_t rssi, void* payload)
{
    assert(payload);
    UpLinkMsg * newUpLinkMsgPtr = NULL;

    newUpLinkMsgPtr = (UpLinkMsg*)OsalMallocFromMemoryPool(sizeof(UpLinkMsg), &s_upLinkMemPool);
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
    assert(data && (length > 0));
    assert(SIZE_OF_PAYLOAD(length) == SIZE_OF_ADV_PAYLOAD);

    char * payload = (char*)OsalMallocFromMemoryPool(SIZE_OF_ADV_PAYLOAD, &s_upLinkMemPool);
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

        status = OsalQueueEnqueue(s_queueOfUpLinkMsg, newUpLinkMsgPtr, &s_upLinkMemPool);
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
    SDK_STAT status = SDK_SUCCESS;

    cJSON *upLinkJson = JsonHeaderCreate();
    if(!upLinkJson)
    {
        return NULL;
    }

    cJSON *timestamp = cJSON_CreateNumber(lastUpLinkMsg->timestamp);
    JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(timestamp,upLinkJson);
    cJSON_AddItemToObject(upLinkJson, "timestamp", timestamp);

    cJSON *sequenceId = cJSON_CreateNumber(lastUpLinkMsg->sequenceId);
    JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(sequenceId,upLinkJson);
    cJSON_AddItemToObject(upLinkJson, "sequenceId", sequenceId);

    cJSON *rssi = cJSON_CreateNumber(lastUpLinkMsg->rssi);
    JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(rssi,upLinkJson);
    cJSON_AddItemToObject(upLinkJson, "rssi", rssi);

    cJSON *payload = cJSON_CreateString(upLinkPayloadString);
    JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(payload,upLinkJson);
    cJSON_AddItemToObject(upLinkJson, "payload", payload);

#ifdef DEBUG
    char* debugString =  cJSON_Print(upLinkJson);
    assert(debugString);
    LOG_DEBUG("\n %s \n",debugString);
    assert(status == SDK_SUCCESS);
    FreeJsonString(debugString);
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

    status = NetSendMQTTPacket((void*) upLinkJson, strlen(upLinkJson)+1);
    assert(status == SDK_SUCCESS);

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
                                        &s_upLinkMemPool, OSAL_FOREVER_TIMEOUT);
        assert(status == SDK_SUCCESS);
        sendLastUpLinkMsg(lastUpLinkMsg);
    }
}

void UpLinkInit(dev_handle devHandle)
{
    s_queueOfUpLinkMsg = OsalQueueCreate(SIZE_OF_UP_LINK_QUEUE);
    assert(s_queueOfUpLinkMsg);
    SDK_STAT sdkStatus = RegisterReceiveCallback(devHandle, bleAdvDataProcessCallback);
	assert(sdkStatus == SDK_SUCCESS);
}