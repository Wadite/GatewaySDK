/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#include <zephyr/types.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>

#include "osal.h"
#include "dev-if.h"
#include "network-api.h"
#include "williotSdkJson.h"
#include "logger.h"
#include "upLink.h"
#include "downLink.h"
#include "sdkConfigurations.h"
#include "mqttTopics.h"
#include "networkManager.h"

void main(void)
{
	dev_handle devHandle;
	SDK_STAT sdkStatus = 0;

	OsalInit(); // System initialization starts here

	sdkStatus = ConfigurationInit();
	assert(sdkStatus == SDK_SUCCESS);

	devHandle = DevInit(DEV_BLE);
	assert(devHandle);

	sdkStatus = JsonHooksInit();
	assert(sdkStatus == SDK_SUCCESS);

	sdkStatus = UpLinkInit(devHandle);
	assert(sdkStatus == SDK_SUCCESS);

	sdkStatus = LoggerInit();
	assert(sdkStatus == SDK_SUCCESS);

	sdkStatus = DownLinkInit(devHandle);
	assert(sdkStatus == SDK_SUCCESS);

	sdkStatus = NetworkManagerInit();
	assert(sdkStatus == SDK_SUCCESS);
	
	OsalStart(); // System therads released here

	OsalSleep(UINT32_MAX);
}
