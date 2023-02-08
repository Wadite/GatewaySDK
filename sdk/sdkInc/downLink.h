#ifndef _DOWN_LINK_H
#define _DOWN_LINK_H

#include "dev-if.h"

/**
 * @brief Initialize the downlink module.
 * 
 * @param dev identifier to identify the specific device.
 * 
 * @return SDK_FAILURE if queue creation failed.
 * @return SDK_SUCCESS if initialized successfully, other received internal error.
 */
SDK_STAT DownLinkInit(dev_handle bleDev, dev_handle localDev);

#endif //_DOWN_LINK_H