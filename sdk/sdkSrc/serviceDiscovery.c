#include <assert.h>
#include "serviceDiscovery.h"
#include "sdk_common.h"
#include <stdbool.h>

#define DOWNLINK_SUPPORT_STATUS_STRING "downlinkSupported"
#define DOWNLINK_SUPPORT_STATUS_VALUE cJSON_True

SDK_STAT AppendServiceDiscovery(cJSON* sdRoot)
{
    if(!sdRoot)
    {
        return SDK_INVALID_PARAMS;
    }
    cJSON* res = cJSON_AddBoolToObject(sdRoot, DOWNLINK_SUPPORT_STATUS_STRING, DOWNLINK_SUPPORT_STATUS_VALUE);
    if(res == NULL)
    {
        return SDK_FAILURE;
    }
    return SDK_SUCCESS;
}