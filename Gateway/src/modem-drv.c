
#include <zephyr/kernel.h>
#include <zephyr/drivers/modem/hl7800.h>

#include "modem-drv.h"
#include "logger.h"

static void modemDeviceStart()
{
    int status = 0;

    printk("Starting modem\n");

    status = mdm_hl7800_reset();
    if (status != 0)
    {
        printk("Failed resetting modem\n");
    }
}

SDK_STAT ModemInit(ReadModemDataCB cb) 
{
    modemDeviceStart();

    return SDK_SUCCESS;
}
