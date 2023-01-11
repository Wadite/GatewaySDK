#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "sdkConfigurations.h"
#include "osal.h"
#include "dev-if.h"

#define MAX_UINT16_HEX_DIGITS           (8)
#define MAX_INT_DIGITS                  (16)
#define MAX_DOUBLE_DIGITS               (32)
#define MAX_CONF_PARAM_SIZE             (16)

#define CONFIG_STRING_DEBUG             "debug"
#define CONFIG_STRING_INFO              "info"
#define CONFIG_STRING_WARNING           "warning"
#define CONFIG_STRING_ERROR             "error"
#define CONFIG_STRING_TRUE              "true"
#define CONFIG_STRING_FALSE             "false"

#define DEFAULT_CONF_UPlOAD_MODE        "online"
#define DEFAULT_CONF_SEVERITY           CONFIG_STRING_INFO
#define DEFAULT_CONF_LOCAL_TRACE        CONFIG_STRING_TRUE
#define DEFAULT_CONF_NUMBER_OF_LOGS     "5"
#define DEFAULT_CONF_UUID               "0xfdaf"
#define DEFAULT_CONF_GATEWAY_ID         "102115004740"
#define DEFAULT_CONF_GATEWAY_TYPE       "none"
#define DEFAULT_CONF_GATEWAY_NAME       "tandemGW"
#define DEFAULT_CONF_LOCATION_SUPPORT   CONFIG_STRING_TRUE
#define DEFAULT_CONF_LOCATION           "0.0"
#define DEFAULT_CONF_MQTT_SERVER        "mqtt-shared-v2-dev.aws.wiliot.com"

static void setLoggerUploadMode(const cJSON *cJson);
static void setLoggerSeverity(const cJSON *cJson);
static void setLoggerLocalTraceConfig(const cJSON *cJson);
static void setLoggerNumberOfLogs(const cJSON *cJson);
static void setUuidToFilter(const cJSON *cJson);
static void setGateWayID(const cJSON *cJson);
static void setGateWayType(const cJSON *cJson);
static void setGateWayName(const cJSON *cJson);
static void setIsLocationSupported(const cJSON *cJson);
static void setLocation(const cJSON *cJson);
static void setMqttServer(const cJSON *cJson);

typedef void(*SetFunction)(const cJSON*);

static const char * s_confParamsTable[CONF_PARAM_NUM] = {
	[CONF_PARAM_LOGGER_UPLOAD_MODE]         = "LoggerUploadMode",
	[CONF_PARAM_LOGGER_SEVERITY]            = "LoggerSeverity",
	[CONF_PARAM_LOGGER_LOCAL_TRACE]         = "LoggerLocalTrace",
	[CONF_PARAM_LOGGER_NUMBER_OF_LOGS]      = "LoggerNumberOfLogs",
    [CONF_PARAM_UUID_TO_FILTER]             = "UUID",
    [CONF_PARAM_GATEWAY_ID]                 = "GateWayID",
	[CONF_PARAM_GATEWAY_TYPE]               = "GateWayType",
	[CONF_PARAM_GATEWAY_NAME]               = "GateWayName",
    [CONF_PARAM_IS_LOCATION_SUPPORTED]      = "LocationSupported",
    [CONF_PARAM_LOCATION]                   = "Location",
    [CONF_PARAM_MQTT_SERVER]                = "MqttServer"
};

static SetFunction s_setParamsFuncPtrTable[CONF_PARAM_NUM] = {
    [CONF_PARAM_LOGGER_UPLOAD_MODE]         = setLoggerUploadMode,
	[CONF_PARAM_LOGGER_SEVERITY]            = setLoggerSeverity,
	[CONF_PARAM_LOGGER_LOCAL_TRACE]         = setLoggerLocalTraceConfig,
	[CONF_PARAM_LOGGER_NUMBER_OF_LOGS]      = setLoggerNumberOfLogs,
    [CONF_PARAM_UUID_TO_FILTER]             = setUuidToFilter,
    [CONF_PARAM_GATEWAY_ID]                 = setGateWayID,
	[CONF_PARAM_GATEWAY_TYPE]               = setGateWayType,
	[CONF_PARAM_GATEWAY_NAME]               = setGateWayName,
    [CONF_PARAM_IS_LOCATION_SUPPORTED]      = setIsLocationSupported,
    [CONF_PARAM_LOCATION]                   = setLocation,
    [CONF_PARAM_MQTT_SERVER]                = setMqttServer,
};

static const char * s_defaultParamsValue[CONF_PARAM_NUM] = {
    [CONF_PARAM_LOGGER_UPLOAD_MODE]         = DEFAULT_CONF_UPlOAD_MODE,
	[CONF_PARAM_LOGGER_SEVERITY]            = DEFAULT_CONF_SEVERITY,
	[CONF_PARAM_LOGGER_LOCAL_TRACE]         = DEFAULT_CONF_LOCAL_TRACE,
	[CONF_PARAM_LOGGER_NUMBER_OF_LOGS]      = DEFAULT_CONF_NUMBER_OF_LOGS,
    [CONF_PARAM_UUID_TO_FILTER]             = DEFAULT_CONF_UUID,
    [CONF_PARAM_GATEWAY_ID]                 = DEFAULT_CONF_GATEWAY_ID,
	[CONF_PARAM_GATEWAY_TYPE]               = DEFAULT_CONF_GATEWAY_TYPE,
	[CONF_PARAM_GATEWAY_NAME]               = DEFAULT_CONF_GATEWAY_NAME,
    [CONF_PARAM_IS_LOCATION_SUPPORTED]      = DEFAULT_CONF_LOCATION_SUPPORT,
    [CONF_PARAM_LOCATION]                   = DEFAULT_CONF_LOCATION,
    [CONF_PARAM_MQTT_SERVER]                = DEFAULT_CONF_MQTT_SERVER,
};

// This parameter is global because it is used in header implementation file 
ConfigurationStruct g_ConfigurationStruct = {0};

static bool s_isConfigurationTableSet = false;

static int stringToInt(char * intStr)
{
    return atoi(intStr);
}

static eLogTypes stringToLogSeverity(char * tempSeverity)
{
    eLogTypes logSeverity = LOG_TYPE_NUM;

    if(strcmp(tempSeverity,CONFIG_STRING_DEBUG) == 0)
    {
        logSeverity = LOG_TYPE_DEBUG;
    }
    else if(strcmp(tempSeverity,CONFIG_STRING_INFO) == 0)
    {
        logSeverity = LOG_TYPE_INFO;
    }
    else if(strcmp(tempSeverity,CONFIG_STRING_WARNING) == 0)
    {
        logSeverity = LOG_TYPE_WARNING;
    }
    else if(strcmp(tempSeverity,CONFIG_STRING_ERROR) == 0)
    {
        logSeverity = LOG_TYPE_ERROR;
    }

    return logSeverity;
}

static bool stringToBool(char * boolStr)
{
    if(strcmp(boolStr, CONFIG_STRING_TRUE) == 0)
    {
        return true;
    }
    else if(strcmp(boolStr, CONFIG_STRING_FALSE) == 0)
    {
        return false;
    }
    else
    {
        assert(true);
        return false; // Doesn't reach here, removed warning
    }
}

static uint16_t stringToUint16(char * uint16Str)
{
    uint16_t res = 0;
    sscanf(uint16Str, "%hx", &res);
    return res;
}

static double stringToDouble(char * doubleStr)
{
    double res = 0;
    sscanf(doubleStr, "%lf", &res);
    return res;
}

static bool setBoolConfigFlashSave(const char * key,char * boolStr)
{
    SDK_STAT status = SDK_SUCCESS;
    bool tempStrBool = false;

    tempStrBool = stringToBool(boolStr);
    status = DevStorageWrite(key, boolStr, strlen(boolStr));
    assert(status == SDK_SUCCESS);

    return tempStrBool;
}

static void setLoggerUploadMode(const cJSON *cJson)
{
    SDK_STAT status = SDK_SUCCESS;
    char * tempLoggerUploadMode = cJSON_GetStringValue(cJson);

    if(g_ConfigurationStruct.loggerUploadMode != NULL)
    {
        OsalFree(g_ConfigurationStruct.loggerUploadMode);
    }

    g_ConfigurationStruct.loggerUploadMode = OsalMalloc(strlen(tempLoggerUploadMode) + 1);
    assert(g_ConfigurationStruct.loggerUploadMode);
    strcpy(g_ConfigurationStruct.loggerUploadMode,tempLoggerUploadMode);

    status = DevStorageWrite(s_confParamsTable[CONF_PARAM_LOGGER_UPLOAD_MODE],
                                g_ConfigurationStruct.loggerUploadMode, strlen(g_ConfigurationStruct.loggerUploadMode));
    assert(status == SDK_SUCCESS);
}

static void setLoggerSeverity(const cJSON *cJson)
{
    SDK_STAT status = SDK_SUCCESS;
    char * tempSeverity = cJSON_GetStringValue(cJson);

    g_ConfigurationStruct.loggerSeverity = stringToLogSeverity(tempSeverity);
    status = DevStorageWrite(s_confParamsTable[CONF_PARAM_LOGGER_SEVERITY],
                            tempSeverity, strlen(tempSeverity));
    assert(status == SDK_SUCCESS);
}

static void setLoggerLocalTraceConfig(const cJSON *cJson)
{
    bool boolResult = false;

    boolResult = setBoolConfigFlashSave(s_confParamsTable[CONF_PARAM_LOGGER_LOCAL_TRACE],
                                            cJSON_GetStringValue(cJson));
    g_ConfigurationStruct.loggerLocalTraceConfig = boolResult;
}

static void setLoggerNumberOfLogs(const cJSON *cJson)
{
    SDK_STAT status = SDK_SUCCESS;
    g_ConfigurationStruct.loggerNumberOfLogs = (int)cJSON_GetNumberValue(cJson);

    char intAsString[MAX_INT_DIGITS] = {0};

    sprintf(intAsString, "%d", g_ConfigurationStruct.loggerNumberOfLogs);
    status = DevStorageWrite(s_confParamsTable[CONF_PARAM_LOGGER_NUMBER_OF_LOGS], 
                                intAsString, strlen(intAsString));
    assert(status == SDK_SUCCESS);
}

static void setUuidToFilter(const cJSON *cJson)
{
    SDK_STAT status = SDK_SUCCESS;
    g_ConfigurationStruct.uuidToFilter = (uint16_t)cJSON_GetNumberValue(cJson);

    char uint16ToHexString[MAX_UINT16_HEX_DIGITS] = {0};
    sprintf(uint16ToHexString,"0x%x",g_ConfigurationStruct.uuidToFilter);

    status = DevStorageWrite(s_confParamsTable[CONF_PARAM_UUID_TO_FILTER],
                                uint16ToHexString, strlen(uint16ToHexString));
    assert(status == SDK_SUCCESS);
}

static void setGateWayID(const cJSON *cJson)
{
    SDK_STAT status = SDK_SUCCESS;
    char * tempGateWayID = cJSON_GetStringValue(cJson);

    if(g_ConfigurationStruct.gateWayId != NULL)
    {
        OsalFree(g_ConfigurationStruct.gateWayId);
    }

    g_ConfigurationStruct.gateWayId = OsalMalloc(strlen(tempGateWayID) + 1);
    assert(g_ConfigurationStruct.gateWayId);
    strcpy(g_ConfigurationStruct.gateWayId,tempGateWayID);

    status = DevStorageWrite(s_confParamsTable[CONF_PARAM_GATEWAY_ID],
                                g_ConfigurationStruct.gateWayId, strlen(g_ConfigurationStruct.gateWayId));
    assert(status == SDK_SUCCESS);
}

static void setGateWayType(const cJSON *cJson)
{
    SDK_STAT status = SDK_SUCCESS;
    char * tempGateWayType = cJSON_GetStringValue(cJson);

    if(g_ConfigurationStruct.gateWayType != NULL)
    {
        OsalFree(g_ConfigurationStruct.gateWayType);
    }

    g_ConfigurationStruct.gateWayType = OsalMalloc(strlen(tempGateWayType) + 1);
    assert(g_ConfigurationStruct.gateWayType);
    strcpy(g_ConfigurationStruct.gateWayType,tempGateWayType);

    status = DevStorageWrite(s_confParamsTable[CONF_PARAM_GATEWAY_TYPE],
                                g_ConfigurationStruct.gateWayType, strlen(g_ConfigurationStruct.gateWayType));
    assert(status == SDK_SUCCESS);
}

static void setGateWayName(const cJSON *cJson)
{
    SDK_STAT status = SDK_SUCCESS;
    char * tempGateWayName = cJSON_GetStringValue(cJson);

    if(g_ConfigurationStruct.gateWayName != NULL)
    {
        OsalFree(g_ConfigurationStruct.gateWayName);
    }

    g_ConfigurationStruct.gateWayName = OsalMalloc(strlen(tempGateWayName) + 1);
    assert(g_ConfigurationStruct.gateWayName);
    strcpy(g_ConfigurationStruct.gateWayName,tempGateWayName);

    status = DevStorageWrite(s_confParamsTable[CONF_PARAM_GATEWAY_NAME],
                                g_ConfigurationStruct.gateWayName, strlen(g_ConfigurationStruct.gateWayName));
    assert(status == SDK_SUCCESS);
}

static void setIsLocationSupported(const cJSON *cJson)
{
    bool boolResult = false;

    boolResult = setBoolConfigFlashSave(s_confParamsTable[CONF_PARAM_IS_LOCATION_SUPPORTED],
                                            cJSON_GetStringValue(cJson));
    g_ConfigurationStruct.isLocationSupported = boolResult;
}

static void setLocation(const cJSON *cJson)
{
    SDK_STAT status = SDK_SUCCESS;
    g_ConfigurationStruct.location = cJSON_GetNumberValue(cJson);
    char doubleAsString[MAX_DOUBLE_DIGITS] = {0};

    sprintf(doubleAsString, "%lf", g_ConfigurationStruct.location);
    status = DevStorageWrite(s_confParamsTable[CONF_PARAM_LOCATION],
                                doubleAsString, strlen(doubleAsString));
    assert(status == SDK_SUCCESS);
}

static void setMqttServer(const cJSON *cJson)
{
    SDK_STAT status = SDK_SUCCESS;
    char * tempMqttServer = cJSON_GetStringValue(cJson);

    if(g_ConfigurationStruct.mqttServer != NULL)
    {
        OsalFree(g_ConfigurationStruct.mqttServer);
    }

    g_ConfigurationStruct.mqttServer = OsalMalloc(strlen(tempMqttServer) + 1);
    assert(g_ConfigurationStruct.mqttServer);
    strcpy(g_ConfigurationStruct.mqttServer,tempMqttServer);

    status = DevStorageWrite(s_confParamsTable[CONF_PARAM_MQTT_SERVER],
                                g_ConfigurationStruct.mqttServer, strlen(g_ConfigurationStruct.mqttServer));
    assert(status == SDK_SUCCESS);
}

static char * readFromStorageAssist(eConfigurationParams confParam)
{
    SDK_STAT status = SDK_SUCCESS;

    char * tempStorageAllocation = OsalMalloc(MAX_CONF_PARAM_SIZE);
    assert(tempStorageAllocation);

    status = DevStorageRead(s_confParamsTable[confParam], tempStorageAllocation, MAX_CONF_PARAM_SIZE);

    if(status == SDK_NOT_FOUND)
    {
        OsalFree(tempStorageAllocation);
        tempStorageAllocation = OsalMalloc(strlen(s_defaultParamsValue[confParam]) + 1);
        strcpy(tempStorageAllocation, s_defaultParamsValue[confParam]);
    }
    else
    {
        assert(status == SDK_SUCCESS);
    }

    return tempStorageAllocation;
}

void ConfigurationInit()
{
    SDK_STAT status = SDK_SUCCESS; 
    status = DevStorageInit();
    assert(status == SDK_SUCCESS);

    // Read params

    // LoggerUploadMode
    char * tempUploadMode = readFromStorageAssist(CONF_PARAM_LOGGER_UPLOAD_MODE);
    g_ConfigurationStruct.loggerUploadMode = tempUploadMode;
    
    // LoggerSeverity
    char * tempSeverity = readFromStorageAssist(CONF_PARAM_LOGGER_SEVERITY);
    g_ConfigurationStruct.loggerSeverity = stringToLogSeverity(tempSeverity);
    OsalFree(tempSeverity);

    // LoggerLocalTrace
    char * tempLocalTrace = readFromStorageAssist(CONF_PARAM_LOGGER_LOCAL_TRACE);
    g_ConfigurationStruct.loggerLocalTraceConfig = stringToBool(tempLocalTrace);
    OsalFree(tempLocalTrace);

    // LoggerNumberOfLogs
    char * tempNumberOfLogs = readFromStorageAssist(CONF_PARAM_LOGGER_NUMBER_OF_LOGS);
    g_ConfigurationStruct.loggerNumberOfLogs = stringToInt(tempNumberOfLogs);
    OsalFree(tempNumberOfLogs);

    // UUID 
    char * tempUUID = readFromStorageAssist(CONF_PARAM_UUID_TO_FILTER);
    g_ConfigurationStruct.uuidToFilter = stringToUint16(tempUUID);
    OsalFree(tempUUID);

    // GateWayID
    char * tempGateWayID = readFromStorageAssist(CONF_PARAM_GATEWAY_ID);
    g_ConfigurationStruct.gateWayId = tempGateWayID;

    // GateWayType 
    char * tempGateWayType = readFromStorageAssist(CONF_PARAM_GATEWAY_TYPE);
    g_ConfigurationStruct.gateWayType = tempGateWayType;

    // GateWayName
    char * tempGateWayName = readFromStorageAssist(CONF_PARAM_GATEWAY_NAME);
    g_ConfigurationStruct.gateWayName = tempGateWayName;

    // LocationSupported
    char * tempLocationSupported = readFromStorageAssist(CONF_PARAM_IS_LOCATION_SUPPORTED);
    g_ConfigurationStruct.isLocationSupported = stringToBool(tempLocationSupported);
    OsalFree(tempLocationSupported);

    // Location 
    char * tempLocation = readFromStorageAssist(CONF_PARAM_LOCATION);
    g_ConfigurationStruct.location = stringToDouble(tempLocation);
    OsalFree(tempLocation);

    // MqttServer
    char * tempMqttServer = readFromStorageAssist(CONF_PARAM_MQTT_SERVER);
    g_ConfigurationStruct.mqttServer = tempMqttServer;

    s_isConfigurationTableSet = true;
}

SDK_STAT SetConfiguration(const cJSON *item)
{
    if(!item)
    {
        return SDK_INVALID_PARAMS;
    }

    if(!s_isConfigurationTableSet)
    {
        return SDK_INVALID_STATE;
    }

    for(eConfigurationParams confParams = CONF_PARAM_LOGGER_UPLOAD_MODE; confParams < CONF_PARAM_NUM; confParams++)
    {
        cJSON * tempJson = cJSON_GetObjectItem(item, s_confParamsTable[confParams]);

        if(!tempJson)
        {
            continue;
        }

        s_setParamsFuncPtrTable[confParams](tempJson);

    }

    return SDK_SUCCESS;
}

bool isConfigurationTableSet()
{
    return s_isConfigurationTableSet;
}
