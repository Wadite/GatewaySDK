#ifndef _UP_LINK_H
#define _UP_LINK_H

#include "dev-if.h"

/**
 * @brief Initialize the uplink module.
 * 
 * @param bleDev identifier to identify the specific ble device.
 * @param localDev identifier to identify the specific local device.
 * 
 * @return SDK_FAILURE if queue creation failed.
 * @return SDK_SUCCESS if initialized successfully, other received internal error.
 */
SDK_STAT UpLinkInit(dev_handle bleDev, dev_handle localDev);

#endif //_UP_LINK_H