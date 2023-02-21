#ifndef _CONFIGURATION_MODULE_IMPLEMENTATION_H_
#define _CONFIGURATION_MODULE_IMPLEMENTATION_H_

#include <stdint.h>
#include <stdbool.h>

#include "logger.h"
#include "sdk_common.h"

typedef struct{
    char *      loggerUploadMode;
    eLogTypes   loggerSeverity;
    bool        loggerLocalTraceConfig;
    int         loggerNumberOfLogs;
    uint16_t    uuidToFilter; 
    char *      accountId;
    char *      gatewayType;
    char *      gatewayId;
    bool        isLocationSupported;
    double      latitude;
    double      longitude;
    char *      mqttServer;
    char *      apiVersion;
}ConfigurationStruct;

bool isConfigurationTableSet();

static inline SDK_STAT GetLoggerUploadMode(const char** loggerUploadModePtr)
{   
    extern ConfigurationStruct g_ConfigurationStruct;
    const ConfigurationStruct * confStructPtr = &g_ConfigurationStruct;

    if(!isConfigurationTableSet())
    {
        return SDK_INVALID_STATE;
    }

    if(!loggerUploadModePtr)
    {
        return SDK_INVALID_PARAMS;
    }

    if(!confStructPtr->loggerUploadMode)
    {
        return SDK_FAILURE;
    }
    
    *loggerUploadModePtr = confStructPtr->loggerUploadMode;
    return SDK_SUCCESS;
}

static inline SDK_STAT GetLoggerSeverity(eLogTypes * loggerSeverity)
{
    extern ConfigurationStruct g_ConfigurationStruct;
    const ConfigurationStruct * confStructPtr = &g_ConfigurationStruct;

    if(!isConfigurationTableSet())
    {
        return SDK_INVALID_STATE;
    }

    if(!loggerSeverity)
    {
        return SDK_INVALID_PARAMS;
    }

    if(confStructPtr->loggerSeverity < 0 || confStructPtr->loggerSeverity >= LOG_TYPE_NUM)
    {
        return SDK_FAILURE;
    }

    *loggerSeverity = confStructPtr->loggerSeverity;
    return SDK_SUCCESS;
}

static inline SDK_STAT GetLoggerLocalTraceConfig(bool * loggerLocalTraceConfig)
{
    extern ConfigurationStruct g_ConfigurationStruct;
    const ConfigurationStruct * confStructPtr = &g_ConfigurationStruct;

    if(!isConfigurationTableSet())
    {
        return SDK_INVALID_STATE;
    }

    if(!loggerLocalTraceConfig)
    {
        return SDK_INVALID_PARAMS;
    }

    *loggerLocalTraceConfig = confStructPtr->loggerLocalTraceConfig;
    return SDK_SUCCESS;
}

static inline SDK_STAT GetLoggerNumberOfLogs(int * loggerNumberOfLogs)
{
    extern ConfigurationStruct g_ConfigurationStruct;
    const ConfigurationStruct * confStructPtr = &g_ConfigurationStruct;

    if(!isConfigurationTableSet())
    {
        return SDK_INVALID_STATE;
    }

    if(!loggerNumberOfLogs)
    {
        return SDK_INVALID_PARAMS;
    }

    if(confStructPtr->loggerNumberOfLogs < 0)
    {
        return SDK_FAILURE;
    }

    *loggerNumberOfLogs = confStructPtr->loggerNumberOfLogs;
    return SDK_SUCCESS;
}

static inline SDK_STAT GetUuidToFilter(uint16_t * uuidToFilter)
{   
    extern ConfigurationStruct g_ConfigurationStruct;
    const ConfigurationStruct * confStructPtr = &g_ConfigurationStruct;

    if(!isConfigurationTableSet())
    {
        return SDK_INVALID_STATE;
    }

    if(!uuidToFilter)
    {
        return SDK_INVALID_PARAMS;
    }

    if(confStructPtr->uuidToFilter == 0)
    {
        return SDK_FAILURE;
    }
    
    *uuidToFilter = confStructPtr->uuidToFilter;
    return SDK_SUCCESS;
}

static inline SDK_STAT GetAccountID(const char** accountId)
{
    extern ConfigurationStruct g_ConfigurationStruct;
    const ConfigurationStruct * confStructPtr = &g_ConfigurationStruct;

    if(!isConfigurationTableSet())
    {
        return SDK_INVALID_STATE;
    }

    if(!accountId)
    {
        return SDK_INVALID_PARAMS;
    }

    if(!confStructPtr->accountId)
    {
        return SDK_FAILURE;
    }

    *accountId = confStructPtr->accountId;
    return SDK_SUCCESS;
}

static inline SDK_STAT GetGatewayType(const char** gatewayType)
{   
    extern ConfigurationStruct g_ConfigurationStruct;
    const ConfigurationStruct * confStructPtr = &g_ConfigurationStruct;

    if(!isConfigurationTableSet())
    {
        return SDK_INVALID_STATE;
    }

    if(!gatewayType)
    {
        return SDK_INVALID_PARAMS;
    }

    if(!confStructPtr->gatewayType)
    {
        return SDK_FAILURE;
    }
    
    *gatewayType = confStructPtr->gatewayType;
    return SDK_SUCCESS;
}

static inline SDK_STAT GetGatewayId(const char** gatewayId)
{   
    extern ConfigurationStruct g_ConfigurationStruct;
    const ConfigurationStruct * confStructPtr = &g_ConfigurationStruct;

    if(!isConfigurationTableSet())
    {
        return SDK_INVALID_STATE;
    }

    if(!gatewayId)
    {
        return SDK_INVALID_PARAMS;
    }

    if(!confStructPtr->gatewayId)
    {
        return SDK_FAILURE;
    }
    
    *gatewayId = confStructPtr->gatewayId;
    return SDK_SUCCESS;
}

static inline SDK_STAT GetIsLocationSupported(bool* isLocationSupported)
{   
    extern ConfigurationStruct g_ConfigurationStruct;
    const ConfigurationStruct * confStructPtr = &g_ConfigurationStruct;

    if(!isConfigurationTableSet())
    {
        return SDK_INVALID_STATE;
    }

    if(!isLocationSupported)
    {
        return SDK_INVALID_PARAMS;
    }
    
    *isLocationSupported = confStructPtr->isLocationSupported;
    return SDK_SUCCESS;
}

static inline SDK_STAT GetLatitude(double * lat)
{   
    extern ConfigurationStruct g_ConfigurationStruct;
    const ConfigurationStruct * confStructPtr = &g_ConfigurationStruct;

    if(!isConfigurationTableSet())
    {
        return SDK_INVALID_STATE;
    }

    if(!lat)
    {
        return SDK_INVALID_PARAMS;
    }

    if(confStructPtr->latitude < 0)
    {
        return SDK_FAILURE;
    }
    
    *lat = confStructPtr->latitude;
    return SDK_SUCCESS;
}

static inline SDK_STAT GetLongitude(double* lng)
{
    extern ConfigurationStruct g_ConfigurationStruct;
    const ConfigurationStruct * confStructPtr = &g_ConfigurationStruct;

    if(!isConfigurationTableSet())
    {
        return SDK_INVALID_STATE;
    }

    if(!lng)
    {
        return SDK_INVALID_PARAMS;
    }

    if(confStructPtr->longitude < 0)
    {
        return SDK_FAILURE;
    }
    
    *lng = confStructPtr->longitude;
    return SDK_SUCCESS;
}

static inline SDK_STAT GetMqttServer(const char ** mqttServer)
{
    extern ConfigurationStruct g_ConfigurationStruct;
    const ConfigurationStruct * confStructPtr = &g_ConfigurationStruct;

    if(!isConfigurationTableSet())
    {
        return SDK_INVALID_STATE;
    }
    
    if(!mqttServer)
    {
        return SDK_INVALID_PARAMS;
    }

    if(!confStructPtr->mqttServer)
    {
        return SDK_FAILURE;
    }
    
    *mqttServer = confStructPtr->mqttServer;
    return SDK_SUCCESS;
}

static inline SDK_STAT GetApiVersion(const char ** apiVersion)
{
    extern ConfigurationStruct g_ConfigurationStruct;
    const ConfigurationStruct * confStructPtr = &g_ConfigurationStruct;

    if(!isConfigurationTableSet())
    {
        return SDK_INVALID_STATE;
    }
    
    if(!apiVersion)
    {
        return SDK_INVALID_PARAMS;
    }

    if(!confStructPtr->apiVersion)
    {
        return SDK_FAILURE;
    }
    
    *apiVersion = confStructPtr->apiVersion;
    return SDK_SUCCESS;
}


#endif //_CONFIGURATION_MODULE_IMPLEMENTATION_H_
