#include <stdint.h>
#include <string.h>

#include "downLink.h"
#include "osal.h"
#include "network-api.h"
#include "cJSON.h"
#include "sdkConfigurations.h"
#include "MQTTPacket.h"
#include "mqttTopics.h"

#define SIZE_OF_DOWN_LINK_QUEUE             (5)
#define SIZE_OF_DOWN_LINK_THREAD            (2048) 
#define SIZE_OF_DOWN_LINK_MEMORY_POOL       (2048)
#define DEFAULT_ADV_DURATION                (100)
#define DEFAULT_ADV_INTERVAL                (57)
#define MQTT_SUCCESS_READ                   (1)
#define JSON_KEY_DURATION                   "txMaxDurationMs"
#define JSON_KEY_RETRIES                    "txMaxRetries"
#define JSON_KEY_PACKET                     "txPacket"
#define ACTION_KEY                          "action"

#define IS_JSON(jsonPtr)                    (*(jsonPtr) == '{')

OSAL_CREATE_POOL(s_downLinkMemPool, SIZE_OF_DOWN_LINK_MEMORY_POOL);

typedef struct{
    char * payload;
    int sizeOfPayload;
} DownLinkMsg;

static void downLinkMsgThreadFunc();
static Queue_t s_queueOfDownLinkMsg = NULL;
static dev_handle s_devHandle = NULL;

OSAL_THREAD_CREATE(downLinkMsgThread, downLinkMsgThreadFunc, SIZE_OF_DOWN_LINK_THREAD, THREAD_PRIORITY_HIGH);

static void freeDownLinkMsg(DownLinkMsg * downLinkMsgPtr)
{
    if(downLinkMsgPtr)
    {
        if(downLinkMsgPtr->payload)
        {
            OsalFreeFromMemoryPool(downLinkMsgPtr->payload, &s_downLinkMemPool);
        }
        OsalFreeFromMemoryPool(downLinkMsgPtr, &s_downLinkMemPool);
    }
}

static void netReceiveMQTTPacketCallback(void * data, uint32_t length)
{
    SDK_STAT status = SDK_SUCCESS;
    char * payload = NULL;
    DownLinkMsg * newDownLinkMsgPtr = NULL;

    assert(data && length);
    
    payload = OsalMallocFromMemoryPool(length, &s_downLinkMemPool);
    if(!payload)
    {
        return;
    }
    memcpy(payload,data,length);
    
    newDownLinkMsgPtr = (DownLinkMsg*)OsalMallocFromMemoryPool(sizeof(DownLinkMsg), &s_downLinkMemPool);
    newDownLinkMsgPtr->payload = payload;
    newDownLinkMsgPtr->sizeOfPayload = length;

    status = OsalQueueEnqueue(s_queueOfDownLinkMsg, newDownLinkMsgPtr, &s_downLinkMemPool);
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

        if(advInterval < DEFAULT_ADV_INTERVAL)
        {
            advInterval = DEFAULT_ADV_INTERVAL;
        }
    }

    jsonNode = cJSON_GetObjectItem(advJson, JSON_KEY_PACKET);
    if(jsonNode)
    {
        advData = cJSON_GetStringValue(jsonNode);
        status = DevSendPacket(s_devHandle, advDuration, advInterval, advData, strlen(advData));
        assert(status == SDK_SUCCESS);
    }
}

static void createDownlinkJson(const char* ptr)
{
    SDK_STAT status = SDK_SUCCESS;
    cJSON * downLinkJson = cJSON_Parse(ptr);

    cJSON * actionJson = cJSON_GetObjectItem(downLinkJson, ACTION_KEY);
    if(actionJson)
    {
        advJson(downLinkJson);
    }
    else
    {
        status = SetConfiguration(downLinkJson);
        assert(status == SDK_SUCCESS);
    }

    cJSON_Delete(downLinkJson);
}

static char* mqttPackageRead(unsigned char* buf,int buflen)
{
    unsigned char dup = 0;
    int qos = 0;
    unsigned char retained = 0;
    unsigned short packetid = 0;
    MQTTString topicName = {0};
    unsigned char * payload = NULL;
    int payloadlen = 0;
    int err = 0;

    err = MQTTDeserialize_publish(&dup, &qos, &retained, &packetid, &topicName,
		                            &payload, &payloadlen, buf, buflen);

    if(err != MQTT_SUCCESS_READ)
    {
        OsalFreeFromMemoryPool(payload, &s_downLinkMemPool);
        return NULL;
    }

    return (char*)payload;
}

static void downLinkProcess(DownLinkMsg * rawDownLink)
{
    char * mqttPayload = NULL;

    mqttPayload = mqttPackageRead(rawDownLink->payload, rawDownLink->sizeOfPayload);

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
                                        &s_downLinkMemPool, OSAL_FOREVER_TIMEOUT);
        assert(status == SDK_SUCCESS);

        downLinkProcess(rawDownLinkMsg);
    }
}

void DownLinkInit(dev_handle dev)
{
    SDK_STAT status = SDK_SUCCESS;

    s_queueOfDownLinkMsg = OsalQueueCreate(SIZE_OF_DOWN_LINK_QUEUE);
    assert(s_queueOfDownLinkMsg);
    status = RegisterNetReceiveMQTTPacketCallback(netReceiveMQTTPacketCallback);
    assert(status == SDK_SUCCESS);
    s_devHandle = dev;

    status = SubscribeToTopic((char*)GetMqttDownlinkTopic());
    assert(status == SDK_SUCCESS);
}
