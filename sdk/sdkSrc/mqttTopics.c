#include <assert.h>
#include <stdio.h>

#include "sdk_common.h"
#include "mqttTopics.h"
#include "sdkConfigurations.h"


#define MQTT_UPLINK_PREFIX					"data-test/"
#define MQTT_DOWNLINK_PREFIX				"update-test/"
#define MQTT_STATUS_PREFIX					"status-test/"
#define SIZE_OF_MQTT_TOPIC					(128)



static char * getMqttTopic(const char * topicPrefix, char * buff)
{
	SDK_STAT status = SDK_SUCCESS;
	int offset = 0;
	const char * gateWayPtr = NULL;

	assert(buff && topicPrefix);
	offset = sprintf(buff, "%s", topicPrefix);
	status = GetAccountID(&gateWayPtr);
	assert(status == SDK_SUCCESS);
    offset += sprintf(buff + offset, "%s", gateWayPtr);
	offset += sprintf(buff + offset, "%s", "/");
	status = GetGatewayId(&gateWayPtr);
	assert(status == SDK_SUCCESS);
    offset += sprintf(buff + offset, "%s", gateWayPtr);

    return buff; 
}

const char * GetMqttUpLinkTopic() 
{
	static char upLinkTopicBuff[SIZE_OF_MQTT_TOPIC] = {0};
	static bool isInitalized = false;

	if(!isInitalized)
	{
		isInitalized = true;
		return getMqttTopic(MQTT_UPLINK_PREFIX, upLinkTopicBuff);
	}

	return upLinkTopicBuff;
}

const char * GetMqttDownlinkTopic() 
{
	static char downLinkTopicBuff[SIZE_OF_MQTT_TOPIC] = {0};
	static bool isInitalized = false;

	if(!isInitalized)
	{
		isInitalized = true;
		return getMqttTopic(MQTT_DOWNLINK_PREFIX, downLinkTopicBuff);
	}

	return downLinkTopicBuff;
}

const char * GetMqttStatusTopic() 
{
	static char settingsTopicBuff[SIZE_OF_MQTT_TOPIC] = {0};
	static bool isInitalized = false;

	if(!isInitalized)
	{
		isInitalized = true;
		return getMqttTopic(MQTT_STATUS_PREFIX, settingsTopicBuff);
	}

	return settingsTopicBuff;
}