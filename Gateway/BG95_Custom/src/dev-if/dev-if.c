#include "dev-if.h"
#include "williotSdkJson.h"
#include "cJSON.h"
#include "logger.h"
#include "flash-drv.h"
#include "sdkUtils.h"

#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>

#define MAX_ADV_DATA_SIZE                   (29)
#define INTERNAL_DEV_ID                     (1)
#define SCAN_INTERVAL                       (0x0010)
#define SCAN_WINDOWN                        (0x0010)
#define ADV_INTERVAL_MAX(ms)	            ((ms)*10/16)
#define ADV_INTERVAL_MIN(ms)	            ((ms)*10/16)
#define STACK_SIZE_OF_ADV                   (512)
#define INITAL_COUNT                        (0)
#define COUNT_LIMIT                         (1)
#define TEN_PRECENT_OF(val)                 ((val)/10)
#define FLASH_BEGIN                         (0)
#define FIRST_JSON_CHAR_INDEX               (0)
#define MIN_ADV_INTERVAL                    (57)
#define MIN_ADV_DURATION                    (1000)
#define SIZE_OF_TYPE_AND_LENGTH             (2)
#define ADV_TYPE_OFFSET                     (1)

#define IS_JSON_NOT_IN_FLASH(flashPage)                 ((flashPage)[FIRST_JSON_CHAR_INDEX] != '{')
#define GET_SIZE_FOR_ALIGNMENT(jsonStringSize)          ((SIZE_OF_WORD - ((jsonStringSize) + 1) % SIZE_OF_WORD))
#define GET_ALIGNED_SIZE_FOR_STRING(jsonStringSize)     (((jsonStringSize) + 1) + GET_SIZE_FOR_ALIGNMENT(jsonStringSize))

void devSendPackThread();

OSAL_THREAD_CREATE(advThread, devSendPackThread, STACK_SIZE_OF_ADV, THREAD_PRIORITY_LOW);
K_SEM_DEFINE(s_sendPackSem, INITAL_COUNT, COUNT_LIMIT);

static dev_handle internalDevHandle = 0;
static bool s_isAdvertising = false;
static struct bt_le_adv_param s_advParams = {0};
static Timer_t s_advTimer = 0;
DevHandlePacketCB s_receivedPacketFuncHandle = 0;

void advTimerCallback()
{
    SDK_STAT sdkStatus = 0;

    k_sem_give(&s_sendPackSem);
    sdkStatus = OsalTimerStop(s_advTimer);
    __ASSERT((sdkStatus == SDK_SUCCESS),"Advertising timer failed to stop");
}

void devSendPackThread()
{
    int err = 0;

    while(true)
    {   
        err = k_sem_take(&s_sendPackSem, K_FOREVER);
        __ASSERT((err == 0),"Semaphore failed to take");
        
        err = bt_le_adv_stop();
        __ASSERT((err == 0),"Advertising failed to stop");
        s_isAdvertising = false;
    }
}

static void scanCb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
		    struct net_buf_simple *buf)
{
    if(s_receivedPacketFuncHandle)
    {
        s_receivedPacketFuncHandle(internalDevHandle,buf->data,buf->len,rssi);
    }
}

dev_handle DevInit(DEV_ID id)
{
	int err = 0;

    if(id != DEV_BLE)
    {
        return NULL;
    }
    
    internalDevHandle = (dev_handle)INTERNAL_DEV_ID;

	struct bt_le_scan_param scan_param = {
		.type       = BT_HCI_LE_SCAN_PASSIVE,
		.options    = BT_LE_SCAN_OPT_NONE,
		.interval   = SCAN_INTERVAL,
		.window     = SCAN_WINDOWN,
	};

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(NULL);
	if (err) 
    {
		LOG_DEBUG_INTERNAL("Bluetooth init failed (err %d)\n", err);
		return NULL;
	}

	err = bt_le_scan_start(&scan_param, scanCb);
	if (err) 
    {
		LOG_DEBUG_INTERNAL("Starting scanning failed (err %d)\n", err);
		return NULL;
	}

    s_advTimer = OsalTimerCreate(advTimerCallback);

    return internalDevHandle;
}


SDK_STAT DevSendPacket(dev_handle dev, uint32_t duration,
                       uint32_t interval, void* data, uint32_t length)
{
    SDK_STAT sdkStatus = 0;
    int err = 0;
    const struct bt_data advBtData[] = {{*((uint8_t*)(data) + ADV_TYPE_OFFSET),  
                                            (length - SIZE_OF_TYPE_AND_LENGTH), 
                                            ((uint8_t*)(data) + SIZE_OF_TYPE_AND_LENGTH)}};

    if(!dev || !duration || !interval || !data || !length || length > (MAX_ADV_DATA_SIZE + SIZE_OF_TYPE_AND_LENGTH))
    {
        return SDK_INVALID_PARAMS;
    }

    // Current hardware support minimum interval of 57, duration 1 sec to increase probability
    interval = MIN_ADV_INTERVAL;
    duration = MIN_ADV_DURATION;

    s_advParams.options = BT_LE_ADV_OPT_NONE;
    s_advParams.interval_max = ADV_INTERVAL_MAX(interval+TEN_PRECENT_OF(interval));
    s_advParams.interval_min = ADV_INTERVAL_MIN(interval-TEN_PRECENT_OF(interval));

    err = bt_le_adv_start(&s_advParams, advBtData, ARRAY_SIZE(advBtData), NULL, 0);
    if (err) 
    {
        LOG_DEBUG_INTERNAL("Advertising failed to start (err %d)\n", err);
        return SDK_FAILURE;
    }

    s_isAdvertising = true;

    sdkStatus = OsalTimerStart(s_advTimer, duration);
    if(sdkStatus != SDK_SUCCESS)
    {
        LOG_DEBUG_INTERNAL("Advertising timer failed to start\n");
        err = bt_le_adv_stop();
        __ASSERT((err == 0),"Advertising failed to stop");
        return SDK_FAILURE;
    }

    return SDK_SUCCESS;
}

bool DevIsBusy(dev_handle dev)
{
    return s_isAdvertising;
}

SDK_STAT RegisterReceiveCallback(dev_handle dev, DevHandlePacketCB cb)
{
    if(!cb || !dev)
    {
        return SDK_INVALID_PARAMS;
    }

    s_receivedPacketFuncHandle = cb;

    return SDK_SUCCESS;
}

logger_local_handle DevLoggerInit()
{
    logger_local_handle unusedHandle = (void*)1;
    return unusedHandle;
}

SDK_STAT DevLoggerSend(logger_local_handle interface_handle, void* data, uint32_t length)
{
    if(!interface_handle || !data || (length == 0))
    {
        return SDK_INVALID_PARAMS;
    }
    printk("%s\n",(char*)data);
    return SDK_SUCCESS;
}

SDK_STAT DevStorageInit()
{
    return FlashInit();
}

SDK_STAT DevStorageRead(const char * key , void * value, size_t sizeOfValue)
{
    SDK_STAT status = SDK_SUCCESS;
    cJSON * flashJson = NULL;
    cJSON * paramJson = NULL;
    uint8_t* pageFromFlash = NULL;
    char * jsonParmString = NULL;
    int sizeOfJsonParmString = 0;

    if(!key || !value || sizeOfValue == 0)
    {
        return SDK_INVALID_PARAMS;
    }
    
    pageFromFlash = OsalMalloc(SIZE_OF_FLASH_PAGE);
    if(!pageFromFlash)
    {
        return SDK_FAILURE;
    }

    status = FlashRead(FLASH_PAGE_CONFIGURATION, pageFromFlash);
    if(status != SDK_SUCCESS)
    {
        OsalFree(pageFromFlash);
        return status;
    }

    if(IS_JSON_NOT_IN_FLASH(pageFromFlash))
    {
        OsalFree(pageFromFlash);
        return SDK_NOT_FOUND;
    }

    flashJson = cJSON_Parse(pageFromFlash);
    OsalFree(pageFromFlash);
    if(!flashJson)
    {
        return SDK_FAILURE;
    }

    paramJson = cJSON_GetObjectItem(flashJson,key);
    if(!paramJson)
    {
        cJSON_Delete(flashJson);
        return SDK_NOT_FOUND;
    }

    jsonParmString = cJSON_GetStringValue(paramJson);
    sizeOfJsonParmString = (strlen(jsonParmString) + 1);
    if(!jsonParmString || (sizeOfJsonParmString > sizeOfValue))
    {
        FreeJsonString(jsonParmString);
        return SDK_INVALID_PARAMS;
    }
    memcpy(value,jsonParmString, sizeOfJsonParmString);
    cJSON_Delete(flashJson);

    return SDK_SUCCESS;
}

static SDK_STAT addNewJsonKeyFlash(const char * key, void * buff, size_t sizeOfBuff, cJSON * root)
{
    cJSON * node = NULL;
    node = cJSON_AddStringToObject(root, key, buff);
    return (node) ? SDK_SUCCESS : SDK_FAILURE;
}

static SDK_STAT addExsistingJsonKeyFlash(const char * key, void * buff, size_t sizeOfBuff, cJSON * root)
{
    bool itemReplaced = false;
    itemReplaced = cJSON_ReplaceItemInObject(root, key, cJSON_CreateString(buff)); // return?
    return (itemReplaced) ? SDK_SUCCESS : SDK_FAILURE;
}

static SDK_STAT jsonToStringZeroPadded(cJSON * root, char ** jsonString, size_t * sizeOfAllocatedString)
{
    char * tempJsonString = cJSON_Print(root);
    int sizeOfJsonString = 0;
    cJSON_Delete(root);

    sizeOfJsonString = strlen(tempJsonString);
    *sizeOfAllocatedString = GET_ALIGNED_SIZE_FOR_STRING(sizeOfJsonString);
    *jsonString = OsalCalloc(*sizeOfAllocatedString);

    if(!(*jsonString))
    {
        FreeJsonString(tempJsonString);
        return SDK_FAILURE;
    }

    memcpy(*jsonString, tempJsonString, strlen(tempJsonString));
    FreeJsonString(tempJsonString);

    return SDK_SUCCESS;
}

static SDK_STAT createNewJsonFlash(const char * key, void * buff, size_t sizeOfBuff)
{
    SDK_STAT status = SDK_SUCCESS;
    char * jsonString = NULL;
    size_t sizeOfAllocatedString = 0;

    status = FlashErase(FLASH_PAGE_CONFIGURATION);
    RETURN_ON_FAIL(status, SDK_SUCCESS, status);

    cJSON *root = cJSON_CreateObject();

    status = addNewJsonKeyFlash(key, buff, sizeOfBuff, root);
    if(status != SDK_SUCCESS)
    {
        cJSON_Delete(root);
        return status;
    }

    status = jsonToStringZeroPadded(root, &jsonString, &sizeOfAllocatedString);
    RETURN_ON_FAIL(status, SDK_SUCCESS, status);

    status = FlashWrite(FLASH_PAGE_CONFIGURATION, (const void *)jsonString, sizeOfAllocatedString);
    OsalFree(jsonString);
    RETURN_ON_FAIL(status, SDK_SUCCESS, status);

    return SDK_SUCCESS;
}

SDK_STAT DevStorageWrite(const char * key, void * value, size_t sizeOfValue)
{
    cJSON * flashJson = NULL;
    uint8_t* pageFromFlash = NULL;
    SDK_STAT status = SDK_SUCCESS;
    char * jsonString = NULL;
    size_t sizeOfAllocatedString = 0;

    if(!key || !value || sizeOfValue == 0)
    {
        return SDK_INVALID_PARAMS;
    }
    
    pageFromFlash = OsalMalloc(SIZE_OF_FLASH_PAGE);
    if(!pageFromFlash)
    {
        return SDK_FAILURE;
    }

    status = FlashRead(FLASH_PAGE_CONFIGURATION, pageFromFlash);
    if(status != SDK_SUCCESS)
    {
        OsalFree(pageFromFlash);
        return status;
    }

    if(IS_JSON_NOT_IN_FLASH(pageFromFlash))
    {
        OsalFree(pageFromFlash);
        return createNewJsonFlash(key,value,sizeOfValue);
    }

    flashJson = cJSON_Parse(pageFromFlash);
    OsalFree(pageFromFlash);
    if(!flashJson)
    {
        return SDK_FAILURE;
    }

    if(!cJSON_HasObjectItem(flashJson, key))
    {
        status = addNewJsonKeyFlash(key, value, sizeOfValue, flashJson);
    }
    else
    {
        status = addExsistingJsonKeyFlash(key, value, sizeOfValue, flashJson);
    }

    if(status != SDK_SUCCESS)
    {
        cJSON_Delete(flashJson);
        return status;
    }

    status = jsonToStringZeroPadded(flashJson, &jsonString, &sizeOfAllocatedString);
    if(status != SDK_SUCCESS)
    {
        return SDK_FAILURE;
    }

    status = FlashErase(FLASH_PAGE_CONFIGURATION);
    if(status != SDK_SUCCESS)
    {
        OsalFree(jsonString);
        return status;
    }

    status = FlashWrite(FLASH_PAGE_CONFIGURATION, (const void *)jsonString, sizeOfAllocatedString);
    OsalFree(jsonString);
    RETURN_ON_FAIL(status, SDK_SUCCESS, status);

    return SDK_SUCCESS;
}