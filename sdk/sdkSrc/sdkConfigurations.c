#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "sdkConfigurations.h"
#include "osal.h"
#include "dev-if.h"
#include "williotSdkJson.h"
#include "networkManager.h"
#include "mqttTopics.h"
#include "sdkUtils.h"
#include "serviceDiscovery.h"

#define DECIMAL_BASE                    (10)
#define HEXADECIMAL_BASE                (16)


#ifndef SIZE_OF_CONFIGURATIONS_MEMORY_POOL
#define SIZE_OF_CONFIGURATIONS_MEMORY_POOL     (4096)
#endif

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
#define DEFAULT_CONF_GATEWAY_TYPE       "other"
#define DEFAULT_CONF_GATEWAY_NAME       "GWtandem"
#define DEFAULT_CONF_LOCATION_SUPPORT   CONFIG_STRING_TRUE
#define DEFAULT_CONF_LOCATION           "0.0"
#define DEFAULT_CONF_MQTT_SERVER        "mqtt-shared-v2-dev.aws.wiliot.com"
#define DEFAULT_API_VERSION             "200"

#define CONFIG_PARAM_JSON_STRING        "gatewayConf"

static void setLoggerUploadMode(const cJSON *cJson);
static void setLoggerSeverity(const cJSON *cJson);
static void setLoggerLocalTraceConfig(const cJSON *cJson);
static void setLoggerNumberOfLogs(const cJSON *cJson);
static void setUuidToFilter(const cJSON *cJson);
static void setGateWayID(const cJSON *cJson);
static void setGateWayType(const cJSON *cJson);
static void setGateWayName(const cJSON *cJson);
static void setIsLocationSupported(const cJSON *cJson);
static void setLatitude(const cJSON *cJson);
static void setLongitude(const cJSON *cJson);
static void setMqttServer(const cJSON *cJson);
static void setApiVersion(const cJSON *cJson);

typedef void(*SetFunction)(const cJSON*);

static const char * s_confParamsTable[CONF_PARAM_NUM] = {
	[CONF_PARAM_LOGGER_UPLOAD_MODE]         = "LoggerUploadMode",
	[CONF_PARAM_LOGGER_SEVERITY]            = "LoggerSeverity",
	[CONF_PARAM_LOGGER_LOCAL_TRACE]         = "LoggerLocalTrace",
	[CONF_PARAM_LOGGER_NUMBER_OF_LOGS]      = "LoggerNumberOfLogs",
    [CONF_PARAM_UUID_TO_FILTER]             = "UUID",
    [CONF_PARAM_GATEWAY_ID]                 = "gatewayID",
	[CONF_PARAM_GATEWAY_TYPE]               = "gatewayType",
	[CONF_PARAM_GATEWAY_NAME]               = "GateWayName",
    [CONF_PARAM_IS_LOCATION_SUPPORTED]      = "LocationSupported",
    [CONF_PARAM_LATITUDE]                   = "lat",
    [CONF_PARAM_LONGITUDE]                  = "lng",
    [CONF_PARAM_MQTT_SERVER]                = "MqttServer",
    [CONF_PARAM_API_VERSION]                = "apiVersion",
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
    [CONF_PARAM_LATITUDE]                   = setLatitude,
    [CONF_PARAM_LONGITUDE]                  = setLongitude,
    [CONF_PARAM_MQTT_SERVER]                = setMqttServer,
    [CONF_PARAM_API_VERSION]                = setApiVersion,
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
    [CONF_PARAM_LATITUDE]                   = DEFAULT_CONF_LOCATION,
    [CONF_PARAM_LONGITUDE]                  = DEFAULT_CONF_LOCATION,
    [CONF_PARAM_MQTT_SERVER]                = DEFAULT_CONF_MQTT_SERVER,
    [CONF_PARAM_API_VERSION]                = DEFAULT_API_VERSION,
};

// This parameter is global because it is used in header implementation file 
ConfigurationStruct g_ConfigurationStruct = {0};

static bool s_isConfigurationTableSet = false;

OSAL_CREATE_POOL(s_configurationsMemoryPool, SIZE_OF_CONFIGURATIONS_MEMORY_POOL);

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

static char * severityToString(eLogTypes loggerSeverity)
{
    switch(loggerSeverity)
    {
        case LOG_TYPE_DEBUG:
            return CONFIG_STRING_DEBUG;
    
        case LOG_TYPE_INFO:
            return CONFIG_STRING_INFO;

        case LOG_TYPE_WARNING:
            return CONFIG_STRING_WARNING;
        
        case LOG_TYPE_ERROR:
            return CONFIG_STRING_ERROR;

        default:
            return NULL;
    }
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
        OsalFreeFromMemoryPool(g_ConfigurationStruct.loggerUploadMode, s_configurationsMemoryPool);
    }

    g_ConfigurationStruct.loggerUploadMode = OsalMallocFromMemoryPool(strlen(tempLoggerUploadMode) + 1, s_configurationsMemoryPool);
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
        OsalFreeFromMemoryPool(g_ConfigurationStruct.gateWayId, s_configurationsMemoryPool);
    }

    g_ConfigurationStruct.gateWayId = OsalMallocFromMemoryPool(strlen(tempGateWayID) + 1, s_configurationsMemoryPool);
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
        OsalFreeFromMemoryPool(g_ConfigurationStruct.gateWayType, s_configurationsMemoryPool);
    }

    g_ConfigurationStruct.gateWayType = OsalMallocFromMemoryPool(strlen(tempGateWayType) + 1, s_configurationsMemoryPool);
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
        OsalFreeFromMemoryPool(g_ConfigurationStruct.gateWayName, s_configurationsMemoryPool);
    }

    g_ConfigurationStruct.gateWayName = OsalMallocFromMemoryPool(strlen(tempGateWayName) + 1, s_configurationsMemoryPool);
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

static void setLatitude(const cJSON *cJson)
{
    SDK_STAT status = SDK_SUCCESS;
    g_ConfigurationStruct.latitude = cJSON_GetNumberValue(cJson);
    char doubleAsString[MAX_DOUBLE_DIGITS] = {0};

    sprintf(doubleAsString, "%lf", g_ConfigurationStruct.latitude);
    status = DevStorageWrite(s_confParamsTable[CONF_PARAM_LATITUDE],
                                doubleAsString, strlen(doubleAsString));
    assert(status == SDK_SUCCESS);
}

static void setLongitude(const cJSON *cJson)
{
    SDK_STAT status = SDK_SUCCESS;
    g_ConfigurationStruct.longitude = cJSON_GetNumberValue(cJson);
    char doubleAsString[MAX_DOUBLE_DIGITS] = {0};

    sprintf(doubleAsString, "%lf", g_ConfigurationStruct.latitude);
    status = DevStorageWrite(s_confParamsTable[CONF_PARAM_LONGITUDE],
                                doubleAsString, strlen(doubleAsString));
    assert(status == SDK_SUCCESS);
}

static void setMqttServer(const cJSON *cJson)
{
    SDK_STAT status = SDK_SUCCESS;
    char * tempMqttServer = cJSON_GetStringValue(cJson);

    if(g_ConfigurationStruct.mqttServer != NULL)
    {
        OsalFreeFromMemoryPool(g_ConfigurationStruct.mqttServer, s_configurationsMemoryPool);
    }

    g_ConfigurationStruct.mqttServer = OsalMallocFromMemoryPool(strlen(tempMqttServer) + 1, s_configurationsMemoryPool);
    assert(g_ConfigurationStruct.mqttServer);
    strcpy(g_ConfigurationStruct.mqttServer,tempMqttServer);

    status = DevStorageWrite(s_confParamsTable[CONF_PARAM_MQTT_SERVER],
                                g_ConfigurationStruct.mqttServer, strlen(g_ConfigurationStruct.mqttServer));
    assert(status == SDK_SUCCESS);
}

static void setApiVersion(const cJSON *cJson)
{
    SDK_STAT status = SDK_SUCCESS;
    char * givenApiVersion = cJSON_GetStringValue(cJson);

    if(g_ConfigurationStruct.apiVersion != NULL)
    {
        OsalFreeFromMemoryPool(g_ConfigurationStruct.apiVersion, s_configurationsMemoryPool);
    }
    g_ConfigurationStruct.apiVersion = OsalMallocFromMemoryPool(strlen(givenApiVersion) + 1, s_configurationsMemoryPool);
    assert(g_ConfigurationStruct.apiVersion);
    strcpy(g_ConfigurationStruct.apiVersion, givenApiVersion);
    status = DevStorageWrite(s_confParamsTable[CONF_PARAM_API_VERSION],
                                g_ConfigurationStruct.apiVersion, strlen(g_ConfigurationStruct.apiVersion));
    assert(status == SDK_SUCCESS);
}

static char * readFromStorageAssist(eConfigurationParams confParam)
{
    SDK_STAT status = SDK_SUCCESS;

    char * tempStorageAllocation = OsalMallocFromMemoryPool(MAX_CONF_PARAM_SIZE, s_configurationsMemoryPool);
    assert(tempStorageAllocation);

    status = DevStorageRead(s_confParamsTable[confParam], tempStorageAllocation, MAX_CONF_PARAM_SIZE);

    if(status == SDK_NOT_FOUND)
    {
        OsalFreeFromMemoryPool(tempStorageAllocation, s_configurationsMemoryPool);
        tempStorageAllocation = OsalMallocFromMemoryPool(strlen(s_defaultParamsValue[confParam]) + 1, s_configurationsMemoryPool);
        strcpy(tempStorageAllocation, s_defaultParamsValue[confParam]);
    }
    else
    {
        assert(status == SDK_SUCCESS);
    }

    return tempStorageAllocation;
}

SDK_STAT ConfigurationInit()
{
    SDK_STAT status = SDK_SUCCESS; 
    status = DevStorageInit();
    RETURN_ON_FAIL(status, SDK_SUCCESS, status);

    // Read params

    // LoggerUploadMode
    char * tempUploadMode = readFromStorageAssist(CONF_PARAM_LOGGER_UPLOAD_MODE);
    g_ConfigurationStruct.loggerUploadMode = tempUploadMode;
    
    // LoggerSeverity
    char * tempSeverity = readFromStorageAssist(CONF_PARAM_LOGGER_SEVERITY);
    g_ConfigurationStruct.loggerSeverity = stringToLogSeverity(tempSeverity);
    OsalFreeFromMemoryPool(tempSeverity, s_configurationsMemoryPool);

    // LoggerLocalTrace
    char * tempLocalTrace = readFromStorageAssist(CONF_PARAM_LOGGER_LOCAL_TRACE);
    g_ConfigurationStruct.loggerLocalTraceConfig = stringToBool(tempLocalTrace);
    OsalFreeFromMemoryPool(tempLocalTrace, s_configurationsMemoryPool);

    // LoggerNumberOfLogs
    char * tempNumberOfLogs = readFromStorageAssist(CONF_PARAM_LOGGER_NUMBER_OF_LOGS);
    g_ConfigurationStruct.loggerNumberOfLogs = stringToInt(tempNumberOfLogs);
    OsalFreeFromMemoryPool(tempNumberOfLogs, s_configurationsMemoryPool);

    // UUID 
    char * tempUUID = readFromStorageAssist(CONF_PARAM_UUID_TO_FILTER);
    g_ConfigurationStruct.uuidToFilter = stringToUint16(tempUUID);
    OsalFreeFromMemoryPool(tempUUID, s_configurationsMemoryPool);

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
    OsalFreeFromMemoryPool(tempLocationSupported, s_configurationsMemoryPool);

    // latitude 
    char * latitude = readFromStorageAssist(CONF_PARAM_LATITUDE);
    g_ConfigurationStruct.latitude = stringToDouble(latitude);
    OsalFreeFromMemoryPool(latitude, s_configurationsMemoryPool);

    // longitude 
    char * longitude = readFromStorageAssist(CONF_PARAM_LONGITUDE);
    g_ConfigurationStruct.longitude = stringToDouble(longitude);
    OsalFreeFromMemoryPool(longitude, s_configurationsMemoryPool);

    // MqttServer
    char * tempMqttServer = readFromStorageAssist(CONF_PARAM_MQTT_SERVER);
    g_ConfigurationStruct.mqttServer = tempMqttServer;
    
    //api version 
    char* apiVersion = readFromStorageAssist(CONF_PARAM_API_VERSION);
    g_ConfigurationStruct.apiVersion = apiVersion;

    s_isConfigurationTableSet = true;

    return SDK_SUCCESS;
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

static void addLocationToJson(cJSON* root)
{
    double loc = 0;
    char locationStr[MAX_CONF_PARAM_SIZE] = {0};
    GetLatitude(&loc);
    sprintf(locationStr, "%lf", loc);
    cJSON_AddStringToObject(root, s_confParamsTable[CONF_PARAM_LATITUDE], locationStr);
    GetLongitude(&loc);
    sprintf(locationStr, "%lf", loc);
    cJSON_AddStringToObject(root, s_confParamsTable[CONF_PARAM_LONGITUDE], locationStr);
}

static const char * getConfigurationJsonString()
{
	cJSON *root = cJSON_CreateObject();
    char * confStr = NULL;

    const char * gateWayId = NULL;
    GetGateWayName(&gateWayId);
    cJSON_AddStringToObject(root, s_confParamsTable[CONF_PARAM_GATEWAY_ID], gateWayId);

    const char * gateWayType = NULL;
    GetGateWayType(&gateWayType);
    cJSON_AddStringToObject(root, s_confParamsTable[CONF_PARAM_GATEWAY_TYPE], gateWayType);

    const char * apiVersion = NULL;
    GetApiVersion(&apiVersion);
    cJSON_AddStringToObject(root, s_confParamsTable[CONF_PARAM_API_VERSION], apiVersion);
    
    cJSON *configPart = cJSON_CreateObject();
    //adding api version twice as request    
    cJSON_AddStringToObject(configPart, s_confParamsTable[CONF_PARAM_API_VERSION], apiVersion);

	const char * loggerUploadModePtr = NULL;
	GetLoggerUploadMode(&loggerUploadModePtr);
	cJSON_AddStringToObject(configPart, s_confParamsTable[CONF_PARAM_LOGGER_UPLOAD_MODE], loggerUploadModePtr);

	eLogTypes loggerSeverity = {0};
	GetLoggerSeverity(&loggerSeverity);
	cJSON_AddStringToObject(configPart, s_confParamsTable[CONF_PARAM_LOGGER_SEVERITY], severityToString(loggerSeverity));

    bool loggerLocalTraceConfig = false;
    GetLoggerLocalTraceConfig(&loggerLocalTraceConfig);
    char * loggerLocalTraceConfigStr = (loggerLocalTraceConfig ? CONFIG_STRING_TRUE : CONFIG_STRING_FALSE);
    cJSON_AddStringToObject(configPart, s_confParamsTable[CONF_PARAM_LOGGER_LOCAL_TRACE], loggerLocalTraceConfigStr);
    
    int loggerNumberOfLogs = 0; 
    GetLoggerNumberOfLogs(&loggerNumberOfLogs);
    char loggerNumberOfLogsStr[MAX_CONF_PARAM_SIZE] = {0};
    __itoa(loggerNumberOfLogs, loggerNumberOfLogsStr, DECIMAL_BASE);
    cJSON_AddStringToObject(configPart, s_confParamsTable[CONF_PARAM_LOGGER_NUMBER_OF_LOGS], loggerNumberOfLogsStr);

    uint16_t uuidToFilter = 0;
    GetUuidToFilter(&uuidToFilter);
    char uuidToFilterStr[MAX_CONF_PARAM_SIZE] = {0};
    __utoa(uuidToFilter, uuidToFilterStr, HEXADECIMAL_BASE);
    cJSON_AddStringToObject(configPart, s_confParamsTable[CONF_PARAM_UUID_TO_FILTER], uuidToFilterStr);

    const char * gateWayName = NULL;
    GetGateWayName(&gateWayName);
    cJSON_AddStringToObject(configPart, s_confParamsTable[CONF_PARAM_GATEWAY_NAME], gateWayName);

    bool isLocationSupported = false;
    GetIsLocationSupported(&isLocationSupported);
    char * isLocationSupportedStr = (isLocationSupported ? CONFIG_STRING_TRUE : CONFIG_STRING_FALSE);
    cJSON_AddStringToObject(configPart, s_confParamsTable[CONF_PARAM_IS_LOCATION_SUPPORTED], isLocationSupportedStr);
    
    addLocationToJson(configPart);

    const char * mqttServer = NULL;
    GetMqttServer(&mqttServer);
    cJSON_AddStringToObject(configPart, s_confParamsTable[CONF_PARAM_MQTT_SERVER], mqttServer);

    cJSON_AddItemToObject(root, CONFIG_PARAM_JSON_STRING, configPart);

    SDK_STAT res = AppendServiceDiscovery(root);
    assert(res == SDK_SUCCESS);

	confStr = cJSON_PrintUnformatted(root);
	cJSON_Delete(root);

    return confStr;
}

SDK_STAT SendConfigurationToServer()
{
    SDK_STAT status = SDK_SUCCESS;
    const char * confStr = getConfigurationJsonString();
    const char * topic = GetMqttStatusTopic();

	status = NetworkMqttMsgSend(topic, (char*)confStr, strlen(confStr));
	FreeJsonString((char*)confStr);

    return status;
}