#ifndef _UP_LINK_H
#define _UP_LINK_H

#include "dev-if.h"

/**
 * @brief Initialize the uplink module.
 * 
 * @param dev identifier to identify the specific device.
 * 
 * @return SDK_FAILURE if queue creation failed.
 * @return SDK_SUCCESS if initialized successfully, other received internal error.
 */
SDK_STAT UpLinkInit(dev_handle devHandle);

#endif //_UP_LINK_H