/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
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

void main(void)
{
	dev_handle devHandle;
	SDK_STAT sdkStatus = 0;

	OsalInit();

	devHandle = DevInit(DEV_BLE);
	assert(devHandle);

	InitJsonHooks();
	sdkStatus = NetworkInit();
	assert(sdkStatus == sdkStatus);

	UpLinkInit(devHandle);
	LoggerInit();

	OsalStart();

	// testing!!!
	connectToNetwork();
	UpdateAccessToken(NULL);

	OsalSleep(UINT32_MAX );
}
