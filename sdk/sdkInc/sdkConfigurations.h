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
    CONF_PARAM_GATEWAY_ID,
    CONF_PARAM_GATEWAY_TYPE,
    CONF_PARAM_GATEWAY_NAME,
    CONF_PARAM_IS_LOCATION_SUPPORTED,
    CONF_PARAM_LATITUDE,
    CONF_PARAM_LONGITUDE,
    CONF_PARAM_MQTT_SERVER,
    CONF_PARAM_API_VERSION,

    CONF_PARAM_NUM,
}eConfigurationParams;

SDK_STAT ConfigurationInit();

SDK_STAT SetConfiguration(const cJSON *item);

SDK_STAT SendConfigurationToServer();

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
inline SDK_STAT GetGateWayID(const char** gateWayId);
inline SDK_STAT GetGateWayType(const char** gateWayType);
inline SDK_STAT GetGateWayName(const char** gateWayName);
inline SDK_STAT GetIsLocationSupported(bool* isLocationSupported);
inline SDK_STAT GetLatitude(double * lat);
inline SDK_STAT GetLongitude(double* lng);
inline SDK_STAT GetMqttServer(const char ** mqttServer);
inline SDK_STAT GetApiVersion(const char ** apiVersion);

#include "sdkConfigurationsImplementation.h"

#endif //_CONFIGURATION_MODULE_H_

