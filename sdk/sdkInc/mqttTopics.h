#ifndef __MQTT_TOPICS_H__
#define __MQTT_TOPICS_H__

/**
 * @brief Function for returning the mqtt topic for uplink
 */
const char * GetMqttUpLinkTopic();

/**
 * @brief Function for returning the mqtt topic for downlink
 */
const char * GetMqttDownlinkTopic();

/**
 * @brief Function for returning the mqtt topic for logs and configuration
 */
const char * GetMqttStatusTopic();

#endif // __MQTT_TOPICS_H__