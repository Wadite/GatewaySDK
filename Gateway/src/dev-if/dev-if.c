#include "dev-if.h"

#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/kernel.h>

#define INTERNAL_DEV_ID         (1)
#define SCAN_INTERVAL           (0x0010)
#define SCAN_WINDOWN            (0x0010)
#define ADV_INTERVAL_MAX(ms)	((ms)*10/16)
#define ADV_INTERVAL_MIN(ms)	((ms)*10/16)
#define STACK_SIZE_OF_ADV       (512)
#define INITAL_COUNT            (0)
#define COUNT_LIMIT             (1)
#define TEN_PRECENT_OF(val)     ((val)/10)

void devSendPackThread();

OSAL_THREAD_CREATE(advThread, devSendPackThread, STACK_SIZE_OF_ADV, THREAD_PRIORITY_LOW);
K_SEM_DEFINE(s_sendPackSem, INITAL_COUNT, COUNT_LIMIT);

static uint8_t s_adDataBuffer[MAX_ADV_DATA_SIZE] = {0};
static const struct bt_data s_advBtData[] = {
    BT_DATA(BT_DATA_LE_SC_RANDOM_VALUE, s_adDataBuffer, MAX_ADV_DATA_SIZE),
};

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

    __ASSERT((id == DEV_BLE),"Received bad dev id");
    
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
		printk("Bluetooth init failed (err %d)\n", err);
		return NULL;
	}

	err = bt_le_scan_start(&scan_param, scanCb);
	if (err) 
    {
		printk("Starting scanning failed (err %d)\n", err);
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

    if(!dev || !duration || !interval || !data || !length || length > MAX_ADV_DATA_SIZE)
    {
        return SDK_INVALID_PARAMS;
    }

    memcpy(s_adDataBuffer, data, length);

    s_advParams.options = BT_LE_ADV_OPT_USE_NAME;
    s_advParams.interval_max = ADV_INTERVAL_MAX(interval+TEN_PRECENT_OF(interval));
    s_advParams.interval_min = ADV_INTERVAL_MIN(interval-TEN_PRECENT_OF(interval));

    err = bt_le_adv_start(&s_advParams, s_advBtData, ARRAY_SIZE(s_advBtData), NULL, 0);
    if (err) 
    {
        printk("Advertising failed to start (err %d)\n", err);
        return SDK_FAILURE;
    }

    s_isAdvertising = true;

    sdkStatus = OsalTimerStart(s_advTimer, duration);
    if(sdkStatus != SDK_SUCCESS)
    {
        printk("Advertising timer failed to start\n");
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