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
#include <zephyr/sys/printk.h>
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

void main(void)
{
	dev_handle devHandle;
	SDK_STAT sdkStatus = 0;

	OsalInit();

	ConfigurationInit();

	devHandle = DevInit(DEV_BLE);
	assert(devHandle);

	InitJsonHooks();
	sdkStatus = NetworkInit();
	assert(sdkStatus == SDK_SUCCESS);

	// testing!!!
	// connectToNetwork();
	// UpdateAccessToken(NULL);
	connectToServer();

	DownLinkInit(devHandle);
	UpLinkInit(devHandle);
	LoggerInit();
	
	OsalStart();

	OsalSleep(UINT32_MAX );
}
