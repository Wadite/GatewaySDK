#include <assert.h>
#include <stdbool.h>
#include "williotSdkJson.h"
#include "osal.h"
#include "sdkConfigurations.h"

#define SIZE_OF_JSON_MEMORY_POOL        (8192)

OSAL_CREATE_POOL(s_jsonMemPool, SIZE_OF_JSON_MEMORY_POOL);

static bool s_isHooksInitialized = false;

static void* mallocForJson(size_t sz)
{
    return OsalMallocFromMemoryPool((uint32_t)sz, s_jsonMemPool);
}

static void freeForJson(void *ptr)
{
    OsalFreeFromMemoryPool(ptr, s_jsonMemPool);
}

SDK_STAT JsonHooksInit()
{
    static cJSON_Hooks jsonHooks = {.free_fn = freeForJson,.malloc_fn = mallocForJson};
    cJSON_InitHooks(&jsonHooks);
    s_isHooksInitialized = true;

    return SDK_SUCCESS;
}

cJSON * JsonHeaderCreate()
{
    SDK_STAT status = SDK_SUCCESS;
    bool cJsonBool = true;
    assert(s_isHooksInitialized);

    cJSON *monitor = cJSON_CreateObject();
    if(!monitor)
    {
        return NULL;
    }

    const char * gatewayIdStr = NULL;
    status = GetGatewayId(&gatewayIdStr);
    assert(status == SDK_SUCCESS);
    cJSON *gatewayId = cJSON_CreateString(gatewayIdStr);
    JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(gatewayId,monitor);
    cJsonBool = cJSON_AddItemToObject(monitor, "gatewayId", gatewayId);
    assert(cJsonBool == true);

    const char * gatewayTypeStr = NULL;
    status = GetGatewayType(&gatewayTypeStr);
    assert(status == SDK_SUCCESS);
    cJSON *gatewayType = cJSON_CreateString(gatewayTypeStr);
    JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(gatewayType,monitor);
    cJsonBool = cJSON_AddItemToObject(monitor, GetConfigurationKeyName(CONF_PARAM_GATEWAY_TYPE), gatewayType);
    assert(cJsonBool == true);

    cJSON *location = cJSON_CreateObject(); // need this?
    JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(location,monitor);
    cJSON_AddItemToObject(monitor, "location", location);

    double latNum = 0;
    status = GetLatitude(&latNum);
    assert(status == SDK_SUCCESS);
    cJSON *lat = cJSON_CreateNumber(latNum);
    JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(lat,monitor);
    cJSON_AddItemToObject(location, GetConfigurationKeyName(CONF_PARAM_LATITUDE), lat);

    double lngNum = 0;
    status = GetLatitude(&lngNum);
    assert(status == SDK_SUCCESS);
    cJSON *lng = cJSON_CreateNumber(lngNum);
    JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(lng,monitor);
    cJSON_AddItemToObject(location, GetConfigurationKeyName(CONF_PARAM_LONGITUDE), lng);

    return monitor;
}

void FreeJsonString(void* ptr)
{
    freeForJson(ptr);
}