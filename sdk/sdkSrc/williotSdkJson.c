#include "williotSdkJson.h"
#include "osal.h"
#include <assert.h>
#include <stdbool.h>


#define SIZE_OF_JSON_MEMORY_POOL        (8192)
// TODO: remove set with real values
#define TEMP_LATITUDE                   (12.34)
#define TEMP_LONGITUDE                  (123.456)

OSAL_CREATE_POOL(s_jsonMemPool, SIZE_OF_JSON_MEMORY_POOL);

static bool s_isHooksInitialized = false;

static void* mallocForJson(size_t sz)
{
    return OsalMallocFromMemoryPool((uint32_t)sz, &s_jsonMemPool);
}

static void freeForJson(void *ptr)
{
    OsalFreeFromMemoryPool(ptr, &s_jsonMemPool);
}

void InitJsonHooks()
{
    static cJSON_Hooks jsonHooks = {.free_fn = freeForJson,.malloc_fn = mallocForJson};
    cJSON_InitHooks(&jsonHooks);
    s_isHooksInitialized = true;
}

cJSON * JsonHeaderCreate()
{
    assert(s_isHooksInitialized);

    char * tempGateWayId = "Gateway Id as string";
    char * tempGatewayType = "Gateway type as string";

    cJSON *monitor = cJSON_CreateObject();
    if(!monitor)
    {
        return NULL;
    }

    cJSON *gatewayId = cJSON_CreateString(tempGateWayId);
    JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(gatewayId,monitor);
    cJSON_AddItemToObject(monitor, "gatewayId", gatewayId);

    cJSON *gatewayType = cJSON_CreateString(tempGatewayType);
    JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(gatewayType,monitor);
    cJSON_AddItemToObject(monitor, "gatewayType", gatewayType);

    cJSON *location = cJSON_CreateObject();
    JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(location,monitor);
    cJSON_AddItemToObject(monitor, "location", location);

    cJSON *lat = cJSON_CreateNumber(TEMP_LATITUDE);
    JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(lat,monitor);
    cJSON_AddItemToObject(location, "lat", lat);

    cJSON *lng = cJSON_CreateNumber(TEMP_LONGITUDE);
    JSON_OBJECT_VERIFY_AND_DELETE_ON_FAIL(lng,monitor);
    cJSON_AddItemToObject(location, "lng", lng);

    return monitor;
}

void FreeJsonString(void* ptr)
{
    OsalFreeFromMemoryPool(ptr, &s_jsonMemPool);
}