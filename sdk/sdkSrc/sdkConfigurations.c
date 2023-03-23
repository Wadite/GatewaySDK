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
#include "network-api.h"

#define DECIMAL_BASE                    (10)
#define HEXADECIMAL_BASE                (16)


#ifndef SIZE_OF_CONFIGURATIONS_MEMORY_POOL
#define SIZE_OF_CONFIGURATIONS_MEMORY_POOL     (4096)
#endif

#define MAX_UINT16_HEX_DIGITS           (8)
#define MAX_INT_DIGITS                  (16)
#define MAX_DOUBLE_DIGITS               (32)
#define MAX_CONF_PARAM_SIZE             (20)
#define MAX_ID_LEN                      (32) //TODO verify sizes


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
#define DEFAULT_CONF_ACCOUNT_ID         "959266658936"/*tandem test:"102115004740"*/
#define DEFAULT_CONF_GATEWAY_TYPE       "lte"
#define DEFAULT_CONF_GATEWAY_ID         "GWDEADBEEF8888"
#define DEFAULT_CONF_LOCATION_SUPPORT   CONFIG_STRING_TRUE
#define DEFAULT_CONF_LOCATION           "0.0"
#define DEFAULT_CONF_MQTT_SERVER        /*test:"mqtt-shared-v2-dev.aws.wiliot.com" prod:*/"mqttv2.wiliot.com"
#define DEFAULT_API_VERSION             "200"

#define CONFIG_PARAM_JSON_STRING        "gatewayConf"

static void setLoggerUploadMode(const cJSON *cJson);
static void setLoggerSeverity(const cJSON *cJson);
static void setLoggerLocalTraceConfig(const cJSON *cJson);
static void setLoggerNumberOfLogs(const cJSON *cJson);
static void setUuidToFilter(const cJSON *cJson);
static void setAccountID(const cJSON *cJson);
static void setGatewayType(const cJSON *cJson);
static void setGatewayId(const cJSON *cJson);
static void setIsLocationSupported(const cJSON *cJson);
static void setLatitude(const cJSON *cJson);
static void setLongitude(const cJSON *cJson);
static void setMqttServer(const cJSON *cJson);
static void setApiVersion(const cJSON *cJson);

typedef void(*SetFunction)(const cJSON*);

static const char * s_confParamsTable[CONF_PARAM_NUM] = {
	[CONF_PARAM_LOGGER_UPLOAD_MODE]         = "loggerUploadMode",
	[CONF_PARAM_LOGGER_SEVERITY]            = "loggerSeverity",
	[CONF_PARAM_LOGGER_LOCAL_TRACE]         = "loggerLocalTrace",
	[CONF_PARAM_LOGGER_NUMBER_OF_LOGS]      = "loggerNumberOfLogs",
    [CONF_PARAM_UUID_TO_FILTER]             = "uuid",
    [CONF_PARAM_ACCOUNT_ID]                 = "accountId",
	[CONF_PARAM_GATEWAY_TYPE]               = "gatewayType",
	[CONF_PARAM_GATEWAY_ID]                 = "gatewayId",
    [CONF_PARAM_IS_LOCATION_SUPPORTED]      = "locationSupported",
    [CONF_PARAM_LATITUDE]                   = "lat",
    [CONF_PARAM_LONGITUDE]                  = "lng",
    [CONF_PARAM_MQTT_SERVER]                = "mqttServer",
    [CONF_PARAM_API_VERSION]                = "apiVersion",
};

static SetFunction s_setParamsFuncPtrTable[CONF_PARAM_NUM] = {
    [CONF_PARAM_LOGGER_UPLOAD_MODE]         = setLoggerUploadMode,
	[CONF_PARAM_LOGGER_SEVERITY]            = setLoggerSeverity,
	[CONF_PARAM_LOGGER_LOCAL_TRACE]         = setLoggerLocalTraceConfig,
	[CONF_PARAM_LOGGER_NUMBER_OF_LOGS]      = setLoggerNumberOfLogs,
    [CONF_PARAM_UUID_TO_FILTER]             = setUuidToFilter,
    [CONF_PARAM_ACCOUNT_ID]                 = setAccountID,
	[CONF_PARAM_GATEWAY_TYPE]               = setGatewayType,
	[CONF_PARAM_GATEWAY_ID]                 = setGatewayId,
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
    [CONF_PARAM_ACCOUNT_ID]                 = DEFAULT_CONF_ACCOUNT_ID,
	[CONF_PARAM_GATEWAY_TYPE]               = DEFAULT_CONF_GATEWAY_TYPE,
	[CONF_PARAM_GATEWAY_ID]                 = DEFAULT_CONF_GATEWAY_ID,
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

static void setAccountID(const cJSON *cJson)
{
    SDK_STAT status = SDK_SUCCESS;
    char * tempAccountID = cJSON_GetStringValue(cJson);

    if(g_ConfigurationStruct.accountId != NULL)
    {
        OsalFreeFromMemoryPool(g_ConfigurationStruct.accountId, s_configurationsMemoryPool);
    }

    g_ConfigurationStruct.accountId = OsalMallocFromMemoryPool(strlen(tempAccountID) + 1, s_configurationsMemoryPool);
    assert(g_ConfigurationStruct.accountId);
    strcpy(g_ConfigurationStruct.accountId,tempAccountID);

    status = DevStorageWrite(s_confParamsTable[CONF_PARAM_ACCOUNT_ID],
                                g_ConfigurationStruct.accountId, strlen(g_ConfigurationStruct.accountId));
    assert(status == SDK_SUCCESS);
}

static void setGatewayType(const cJSON *cJson)
{
    SDK_STAT status = SDK_SUCCESS;
    char * tempGatewayType = cJSON_GetStringValue(cJson);

    if(g_ConfigurationStruct.gatewayType != NULL)
    {
        OsalFreeFromMemoryPool(g_ConfigurationStruct.gatewayType, s_configurationsMemoryPool);
    }

    g_ConfigurationStruct.gatewayType = OsalMallocFromMemoryPool(strlen(tempGatewayType) + 1, s_configurationsMemoryPool);
    assert(g_ConfigurationStruct.gatewayType);
    strcpy(g_ConfigurationStruct.gatewayType,tempGatewayType);

    status = DevStorageWrite(s_confParamsTable[CONF_PARAM_GATEWAY_TYPE],
                                g_ConfigurationStruct.gatewayType, strlen(g_ConfigurationStruct.gatewayType));
    assert(status == SDK_SUCCESS);
}

static void setGatewayId(const cJSON *cJson)
{
    SDK_STAT status = SDK_SUCCESS;
    char * tempGatewayId = cJSON_GetStringValue(cJson);

    if(g_ConfigurationStruct.gatewayId != NULL)
    {
        OsalFreeFromMemoryPool(g_ConfigurationStruct.gatewayId, s_configurationsMemoryPool);
    }

    g_ConfigurationStruct.gatewayId = OsalMallocFromMemoryPool(strlen(tempGatewayId) + 1, s_configurationsMemoryPool);
    assert(g_ConfigurationStruct.gatewayId);
    strcpy(g_ConfigurationStruct.gatewayId,tempGatewayId);

    status = DevStorageWrite(s_confParamsTable[CONF_PARAM_GATEWAY_ID],
                                g_ConfigurationStruct.gatewayId, strlen(g_ConfigurationStruct.gatewayId));
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

SDK_STAT UpdateConfGatewayId(void)
{
    SDK_STAT status = SDK_SUCCESS;
    cJSON *tempJson = NULL;
    char *gatewayId = NULL;
    char *imei = NULL;

    imei = GetIMEI();
    if (NULL == imei)
    {
        status = SDK_FAILURE;
        goto clean_up;
    }

    gatewayId = OsalMallocFromMemoryPool(MAX_ID_LEN, s_configurationsMemoryPool);
    if (NULL == gatewayId)
    {
        status = SDK_FAILURE;
        goto clean_up;
    }
    sprintf(gatewayId, "LTE%s", imei);

    tempJson = cJSON_CreateString(gatewayId);
    if (NULL == tempJson)
    {
        status = SDK_FAILURE;
        goto clean_up;
    }

    setGatewayId(tempJson);

clean_up:
    if (tempJson)
    {
        cJSON_free(tempJson);
    }
    if (gatewayId)
    {
        OsalFreeFromMemoryPool(gatewayId, s_configurationsMemoryPool);
    }

    return status;
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
    char * tempGateWayID = readFromStorageAssist(CONF_PARAM_ACCOUNT_ID);
    g_ConfigurationStruct.accountId = tempGateWayID;

    // GatewayType 
    char * tempGatewayType = readFromStorageAssist(CONF_PARAM_GATEWAY_TYPE);
    g_ConfigurationStruct.gatewayType = tempGatewayType;

    // GatewayId
    char * tempGatewayId = readFromStorageAssist(CONF_PARAM_GATEWAY_ID);
    g_ConfigurationStruct.gatewayId = tempGatewayId;

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
    GetLatitude(&loc);
    cJSON_AddNumberToObject(root, s_confParamsTable[CONF_PARAM_LATITUDE], loc);
    GetLongitude(&loc);
    cJSON_AddNumberToObject(root, s_confParamsTable[CONF_PARAM_LONGITUDE], loc);
}

static const char * getConfigurationJsonString()
{
	cJSON *root = cJSON_CreateObject();
    char * confStr = NULL;

    const char * gatewayId = NULL;
    GetGatewayId(&gatewayId);
    cJSON_AddStringToObject(root, s_confParamsTable[CONF_PARAM_GATEWAY_ID], gatewayId);

    const char * gatewayType = NULL;
    GetGatewayType(&gatewayType);
    cJSON_AddStringToObject(root, s_confParamsTable[CONF_PARAM_GATEWAY_TYPE], gatewayType);

    const char * apiVersion = NULL;
    GetApiVersion(&apiVersion);
    cJSON_AddStringToObject(root, s_confParamsTable[CONF_PARAM_API_VERSION], apiVersion);
    
    cJSON *configPart = cJSON_CreateObject();
    //adding api version twice as request    
    cJSON_AddStringToObject(configPart, s_confParamsTable[CONF_PARAM_API_VERSION], apiVersion);

    cJSON_AddStringToObject(configPart, "interfaceChipSwVersion", "3.11.36");   // for config edit on web
    cJSON_AddStringToObject(configPart, "bleChipSwVersion", "3.11.40");         // for config edit on web

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

    const char * accountId = NULL;
    GetAccountID(&accountId);
    cJSON_AddStringToObject(configPart, s_confParamsTable[CONF_PARAM_ACCOUNT_ID], accountId);

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

    printk("Cfg json sent:\n%s\ntopic:%s\n", confStr, topic);

	status = NetworkMqttMsgSend(topic, (char*)confStr, strlen(confStr));
	FreeJsonString((char*)confStr);

    return status;
}

const char * GetConfigurationKeyName(eConfigurationParams confParm)
{
    if(confParm >= CONF_PARAM_NUM)
    {
        return NULL;
    }

    return s_confParamsTable[confParm];
}