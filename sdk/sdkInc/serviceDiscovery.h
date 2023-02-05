#ifndef __SERVICE_DISCOVERY_H__
#define __SERVICE_DISCOVERY_H__

#include "cJSON.h"
#include "sdk_common.h"

/**
 * @brief appends the service discovery to the given cson object
 * 
 * returns success or other error code on fail
 */
SDK_STAT AppendServiceDiscovery(cJSON* sdRoot);


#endif // __SERVICE_DISCOVERY_H__