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
    uint16_t    uuidToFilter; // think about this
    char *      gateWayId;
    char *      gateWayType;
    char *      gateWayName;
    bool        isLocationSupported;
    double      location;
    char *      mqttServer;
}ConfigurationStruct;

inline SDK_STAT GetLoggerUploadMode(const char** loggerUploadModePtr)
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

inline SDK_STAT GetLoggerSeverity(eLogTypes * loggerSeverity)
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

inline SDK_STAT GetLoggerLocalTraceConfig(bool * loggerLocalTraceConfig)
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

inline SDK_STAT GetLoggerNumberOfLogs(int * loggerNumberOfLogs)
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

inline SDK_STAT GetUuidToFilter(uint16_t * uuidToFilter)
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

inline SDK_STAT GetGateWayID(const char** gateWayId)
{
    extern ConfigurationStruct g_ConfigurationStruct;
    const ConfigurationStruct * confStructPtr = &g_ConfigurationStruct;

    if(!isConfigurationTableSet())
    {
        return SDK_INVALID_STATE;
    }

    if(!gateWayId)
    {
        return SDK_INVALID_PARAMS;
    }

    if(!confStructPtr->gateWayId)
    {
        return SDK_FAILURE;
    }

    *gateWayId = confStructPtr->gateWayId;
    return SDK_SUCCESS;
}

inline SDK_STAT GetGateWayType(const char** gateWayType)
{   
    extern ConfigurationStruct g_ConfigurationStruct;
    const ConfigurationStruct * confStructPtr = &g_ConfigurationStruct;

    if(!isConfigurationTableSet())
    {
        return SDK_INVALID_STATE;
    }

    if(!gateWayType)
    {
        return SDK_INVALID_PARAMS;
    }

    if(!confStructPtr->gateWayType)
    {
        return SDK_FAILURE;
    }
    
    *gateWayType = confStructPtr->gateWayType;
    return SDK_SUCCESS;
}

inline SDK_STAT GetGateWayName(const char** gateWayName)
{   
    extern ConfigurationStruct g_ConfigurationStruct;
    const ConfigurationStruct * confStructPtr = &g_ConfigurationStruct;

    if(!isConfigurationTableSet())
    {
        return SDK_INVALID_STATE;
    }

    if(!gateWayName)
    {
        return SDK_INVALID_PARAMS;
    }

    if(!confStructPtr->gateWayName)
    {
        return SDK_FAILURE;
    }
    
    *gateWayName = confStructPtr->gateWayName;
    return SDK_SUCCESS;
}

inline SDK_STAT GetIsLocationSupported(bool* isLocationSupported)
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

inline SDK_STAT GetLocation(double * location)
{   
    extern ConfigurationStruct g_ConfigurationStruct;
    const ConfigurationStruct * confStructPtr = &g_ConfigurationStruct;

    if(!isConfigurationTableSet())
    {
        return SDK_INVALID_STATE;
    }

    if(!location)
    {
        return SDK_INVALID_PARAMS;
    }

    if(confStructPtr->location < 0)
    {
        return SDK_FAILURE;
    }
    
    *location = confStructPtr->location;
    return SDK_SUCCESS;
}

inline SDK_STAT GetMqttServer(const char ** mqttServer)
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

#endif //_CONFIGURATION_MODULE_IMPLEMENTATION_H_
