#include "sdk_common.h"
#include "network-api.h"
#include "networkManager.h"
#include "osal.h"
#include "cJSON.h"
#include "sdkConfigurations.h"
#include "sdkUtils.h"
#include "mqttTopics.h"

#include <string.h>

#ifndef SIZE_OF_NETWORK_MANAGER_THREAD
#define SIZE_OF_NETWORK_MANAGER_THREAD           (2048) 
#endif

#ifndef SIZE_OF_NETWORK_MANAGER_MEMORY
#ifdef DYNAMIC_ALLOCATION_USED
#define SIZE_OF_NETWORK_MANAGER_MEMORY           (128)
#else
#define SIZE_OF_NETWORK_MANAGER_MEMORY           (1024)
#endif  
#endif

#ifndef SIZE_OF_NETWORK_MANGAER_QUEUE
#define SIZE_OF_NETWORK_MANGAER_QUEUE            (1)
#endif

#define ACCESS_TOKEN_REFRESH_WINDOW_MS           (300000)
#define RECONNECTION_ATTEMPTS                    (5)

OSAL_CREATE_POOL(s_networkManagerMemPool, SIZE_OF_NETWORK_MANAGER_MEMORY);

typedef enum{
	CONN_STATE_DISCONNECTION,

    CONN_STATE_NUM
} eConnectionStates;

static conn_handle s_connHandle = 0;
static Queue_t s_queueOfNetworkManager = NULL;
static bool s_isNetworkConnected = false;
static Mutex_t s_mqttMsgMutex = 0;
static uint32_t s_accessTokenExpiry = 0;

static void networkManagerThreadFunc();
static SDK_STAT fullConnectionInit();

OSAL_THREAD_CREATE(networkManagerThread, networkManagerThreadFunc, SIZE_OF_NETWORK_MANAGER_THREAD, THREAD_PRIORITY_HIGH);

static void changeConnStateCallback(conn_handle connHandle)
{
    SDK_STAT status = SDK_SUCCESS;
    eConnectionStates * connState = NULL;

    assert(connHandle == s_connHandle);
    
    connState = (eConnectionStates*)OsalMallocFromMemoryPool(sizeof(eConnectionStates), s_networkManagerMemPool);
    if(!connState)
    {
        return;
    }

    *connState = CONN_STATE_DISCONNECTION;

    status = OsalQueueEnqueue(s_queueOfNetworkManager, connState, s_networkManagerMemPool);
    if(status != SDK_SUCCESS)
    {
        OsalFreeFromMemoryPool(connState, s_networkManagerMemPool);
    }
}

static SDK_STAT localAccessTokenUpdate(Token refreshToken)
{
    SDK_STAT status = SDK_SUCCESS;
    Token accessToken = NULL;

    status = UpdateAccessToken(refreshToken, &accessToken, &s_accessTokenExpiry);
    RETURN_ON_FAIL(status, SDK_SUCCESS, status);

    s_accessTokenExpiry += (OsalGetTime() - ACCESS_TOKEN_REFRESH_WINDOW_MS);

    return SDK_SUCCESS;
}

static SDK_STAT httpFlow(Token * refreshToken)
{
    SDK_STAT status = SDK_SUCCESS;

    status = GetRefreshToken(refreshToken);
    if(status == SDK_NOT_FOUND)
    {
        status = ConnectToNetwork();
        printk("connecttonetwork %d\n", status);
        RETURN_ON_FAIL(status, SDK_SUCCESS, status);

        status = GetRefreshToken(refreshToken);
        printk("getrefreshtoken %d\n", status);
        RETURN_ON_FAIL(status, SDK_SUCCESS, status);

        status = UpdateRefreshToken(*refreshToken);
        printk("updaterefreshtoken %d\n", status);
        RETURN_ON_FAIL(status, SDK_SUCCESS, status);
    }

    return SDK_SUCCESS;
}

static SDK_STAT mqttFlow(Token refreshToken)
{
    SDK_STAT status = SDK_SUCCESS;

    status = localAccessTokenUpdate(refreshToken);
    RETURN_ON_FAIL(status, SDK_SUCCESS, status);

    s_connHandle = ConnectToServer();
    if(!s_connHandle)
    {
        return SDK_FAILURE;
    }

    return SDK_SUCCESS;
}

static SDK_STAT reconnectToMqtt()
{
    SDK_STAT status = SDK_SUCCESS;
    Token * refreshToken = NULL;

    if(!IsNetworkAvailable())
    {
        return SDK_INVALID_STATE;
    }

    status = GetRefreshToken(refreshToken);
    RETURN_ON_FAIL(status, SDK_SUCCESS, status);

    status = mqttFlow(*refreshToken);
    RETURN_ON_FAIL(status, SDK_SUCCESS, status);

    return SDK_SUCCESS;
}

static void reconnectionFlow()
{
    SDK_STAT status = SDK_SUCCESS;
    uint8_t attempts  = 0;

    s_isNetworkConnected = false;
    
    status = reconnectToMqtt();
    while((status != SDK_SUCCESS) && attempts < RECONNECTION_ATTEMPTS)
    {
        attempts++;
        status = ReconnectToNetwork();

        if(status == SDK_SUCCESS)
        {
            status = reconnectToMqtt();
        }
    }

    if(attempts == RECONNECTION_ATTEMPTS && status != SDK_SUCCESS)
    {
        OsalSystemReset();
    }

    s_isNetworkConnected = true;
}

static void networkManagerThreadFunc()
{
    SDK_STAT status = SDK_SUCCESS;
    eConnectionStates * connState = NULL;

    status = fullConnectionInit();
    assert(SDK_SUCCESS == status);

    status = SubscribeToTopic((char*)GetMqttDownlinkTopic());
    assert(SDK_SUCCESS == status);

    s_isNetworkConnected = true;

    status = SendConfigurationToServer();
    assert(SDK_SUCCESS == status);

    s_isNetworkConnected = true;
    while(true)
    {
        status = OsalQueueWaitForObject(s_queueOfNetworkManager, 
                                        ((void **)(&(connState))),
                                        s_networkManagerMemPool, OSAL_FOREVER_TIMEOUT);
        assert(status == SDK_SUCCESS);

        switch(*connState)
        {
            case CONN_STATE_DISCONNECTION:
                reconnectionFlow();
                break;

            case CONN_STATE_NUM:
                break;
        }

        OsalFreeFromMemoryPool(connState, s_networkManagerMemPool);
    }
}

static SDK_STAT fullConnectionInit()
{
    SDK_STAT status = SDK_SUCCESS;
    Token refreshToken = NULL;

    status = httpFlow(&refreshToken);
    printk("httpflow %d\n", status);
    RETURN_ON_FAIL(status, SDK_SUCCESS, status);

    status = mqttFlow(refreshToken);
    printk("mqttflow %d\n", status);
    RETURN_ON_FAIL(status, SDK_SUCCESS, status);

    return SDK_SUCCESS;
}

SDK_STAT NetworkManagerInit()
{
    SDK_STAT status = SDK_SUCCESS;

    #ifdef DYNAMIC_ALLOCATION_USED
    s_mqttMsgMutex = OsalMutexCreate();
    #else
    s_mqttMsgMutex = OsalMutexCreate(s_networkManagerMemPool);
    #endif

    if(!s_mqttMsgMutex)
    {
        return SDK_FAILURE;
    }

    #ifdef DYNAMIC_ALLOCATION_USED
    s_queueOfNetworkManager = OsalQueueCreate(SIZE_OF_NETWORK_MANGAER_QUEUE);
    #else
    s_queueOfNetworkManager = OsalQueueCreate(SIZE_OF_NETWORK_MANGAER_QUEUE, s_networkManagerMemPool);
    #endif

    if(!s_queueOfNetworkManager)
    {
        return SDK_FAILURE;
    }

    status = RegisterConnStateChangeCB(changeConnStateCallback, s_connHandle);
    RETURN_ON_FAIL(status, SDK_SUCCESS, status);

    return SDK_SUCCESS;
}

SDK_STAT NetworkMqttMsgSend(const char* topic, void* pkt, uint32_t length)
{
    SDK_STAT status = SDK_SUCCESS;

    status = OsalMutexLock(s_mqttMsgMutex, 0);
    assert(status == SDK_SUCCESS);
    
    if(s_isNetworkConnected)
    {
        if(OsalGetTime() >= s_accessTokenExpiry)
        {
            Token refreshToken = 0;
            status = GetRefreshToken(&refreshToken);
            RETURN_ON_FAIL(status, SDK_SUCCESS, status);

            status = localAccessTokenUpdate(refreshToken);
            RETURN_ON_FAIL(status, SDK_SUCCESS, status);
        }

        status = NetSendMQTTPacket(topic,pkt, length);
        RETURN_ON_FAIL(status, SDK_SUCCESS, status);
    }

    status = OsalMutexUnlock(s_mqttMsgMutex);
    assert(status == SDK_SUCCESS);

    return SDK_SUCCESS;
}
