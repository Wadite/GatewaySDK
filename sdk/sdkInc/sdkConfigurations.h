#ifndef _CONFIGURATION_MODULE_H_
#define _CONFIGURATION_MODULE_H_

#include <stdint.h>
#include <stdbool.h>

#include "sdk_common.h"
#include "logger.h"
#include "cJSON.h"

typedef enum{
    CONF_PARAM_LOGGER_UPLOAD_MODE,
    CONF_PARAM_LOGGER_SEVERITY,
    CONF_PARAM_LOGGER_LOCAL_TRACE,
    CONF_PARAM_LOGGER_NUMBER_OF_LOGS,
    CONF_PARAM_UUID_TO_FILTER,
    CONF_PARAM_ACCOUNT_ID,
    CONF_PARAM_GATEWAY_TYPE,
    CONF_PARAM_GATEWAY_ID,
    CONF_PARAM_IS_LOCATION_SUPPORTED,
    CONF_PARAM_LATITUDE,
    CONF_PARAM_LONGITUDE,
    CONF_PARAM_MQTT_SERVER,
    CONF_PARAM_API_VERSION,

    CONF_PARAM_NUM,
}eConfigurationParams;

/**
 * @brief Initialize the global configuration table, by reading
 *        key values from storage, or by using default values.
 * 
 * @return SDK_SUCCESS if all configurations was initialized successfully 
 */
SDK_STAT ConfigurationInit();

/**
 * @brief Set new value to requested key in configuration table.
 * 
 * @param item pointer to cJson object that has the reqeusted key value pair.
 * 
 * @return SDK_INVALID_PARAMS if null pointer was passed
 * @return SDK_INVALID_STATE if configuration table has not been set.
 * @return SDK_SUCCESS if changed successfully 
 */
SDK_STAT SetConfiguration(const cJSON *item);

/**
 * @brief Send the Configuratin table as string to server
 * 
 * @return SDK_SUCCESS if sent successfully 
 */
SDK_STAT SendConfigurationToServer();

/**
 * @brief key name of the configuration parameter.
 * 
 * @param confParm the enum of requested key.
 * 
 * @return NULL if bad param was passed, otherwise string of the key name
 */
const char * GetConfigurationKeyName(eConfigurationParams confParm);

/**
 * @brief Inline functions to get requested param from configurations
 * 
 * @param ConfigName constant pointer for getting requested config value.
 * 
 * @return SDK_INVALID_PARAMS if null pointer was passed
 * @return SDK_INVALID_STATE if configuration has not been set.
 * @return SDK_FAILURE if requested configuration has invalid value.
 * @return SDK_SUCCESS if requested param was written to poiner successfully
 */
inline SDK_STAT GetLoggerUploadMode(const char** loggerUploadModePtr);
inline SDK_STAT GetLoggerSeverity(eLogTypes * loggerSeverityPtr);
inline SDK_STAT GetLoggerLocalTraceConfig(bool * loggerLocalTraceConfig);
inline SDK_STAT GetLoggerNumberOfLogs(int * loggerNumberOfLogs);
inline SDK_STAT GetUuidToFilter(uint16_t * uuidToFilter);
inline SDK_STAT GetAccountID(const char** accountId);
inline SDK_STAT GetGatewayType(const char** gatewayType);
inline SDK_STAT GetGatewayId(const char** gatewayId);
inline SDK_STAT GetIsLocationSupported(bool* isLocationSupported);
inline SDK_STAT GetLatitude(double * lat);
inline SDK_STAT GetLongitude(double* lng);
inline SDK_STAT GetMqttServer(const char ** mqttServer);
inline SDK_STAT GetApiVersion(const char ** apiVersion);

#include "sdkConfigurationsImplementation.h"

#endif //_CONFIGURATION_MODULE_H_

