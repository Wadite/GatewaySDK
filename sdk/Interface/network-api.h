#ifndef _NETWORK_API_H_
#define _NETWORK_API_H_

#include <stdint.h>
#include <stdbool.h>

#include "sdk_common.h"

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief the Network abstraction module is responsible to manage connectivity authorization tokens.
 * 
 */

typedef void* net_handle;
typedef void* conn_handle;
typedef void* Token;

/**
 * @brief initialize the Network stack and make sure it is ready to connect to the network.
 * 
 * @return SDK_SUCCESS upon success, relevant error otherwise
 */
SDK_STAT NetworkInit();


/**
 * @brief establish network connection
 * cellular - register to network and get valid PDP context.
 * WiFi - connect to SSID and get IP settings
 * Eth - complete DHCP and have valid IP settings
 * 
 * @return SDK_SUCCESS upon success, relevant error otherwise
 */
SDK_STAT connectToNetwork();

/**
 * @brief returns if a network is available.
 * cellular - registered to network and has data link.
 * WiFi - connected to SSID and has IP settings.
 * Eth - interface up after autoNeg and has IP settings.
 * 
 * @return true if network is available
 * @return false otherwise
 */
bool IsNetworkAvailable();

/**
 * @brief complete MQTT connection to Wiliot broker and topic subscription
 * 
 * @return NULL conn_handle upon failure. Valid handle upon success.
 */
conn_handle connectToServer();

/**
 * @brief has a valid MQTT connection to Wiliot broker.
 * Can be implemented by tracking logic state or by accessing underlying layers.
 * 
 * @return conn_handle 
 */
conn_handle IsConnectedToBrokerServer();

/**
 * @brief A callback type to be called upon completion of connection to MQTT or disconnection from broker.
 * Implementation should be short and non-blocking.
 */
typedef void (*handleConnStateChangeCB)(conn_handle handle);


/**
 * @brief register the handleConnStateChangeCB callback so it will be called when needed.
 * 
 * @param cb The callback to be registered
 * @param handle the connection handle to track
 * 
 * @return SDK_SUCCESS upon success, relevant error otherwise
 */
SDK_STAT RegisterConnStateChangeCB(handleConnStateChangeCB cb, conn_handle handle);


/**
 * @brief  Publish a message on the MQTT topic, expects complete JSON.
 * This API is synchronous, return of the API means packet was sent.
 * 
 * @param pkt complete valid JSON payload of the MQTT message
 * @param length length of the JSON message.
 * 
 * @return SDK_SUCCESS upon success, relevant error otherwise
 */
SDK_STAT NetSendMQTTPacket(void* pkt, uint32_t length);

/**
 * @brief gets an MQTT message on the subscribed topic, extracts JSON payload and call DnlnkPkt()
 * 
 * @param pkt complete valid JSON payload of the MQTT message
 * @param length length of the JSON message.
 * @return SDK_SUCCESS upon success, relevant error otherwise 
 */
SDK_STAT NetHandlePacket(void* pkt, uint32_t length);

/**
 * @brief Get the Access Token object
 * 
 * @param [out] accessToken pointer
 * @return SDK_SUCCESS upon success, relevant error otherwise  
 */
SDK_STAT GetAccessToken(Token* accessToken);

/**
 * @brief write the access token to persistent storage
 * 
 * @param accessToken 
 * @return SDK_SUCCESS upon success, relevant error otherwise 
 */
SDK_STAT UpdateAccessToken(Token accessToken);

/**
 * @brief Get the Refresh Token object
 * 
 * @param [out] refreshToken pointer
 * @return SDK_SUCCESS upon success, relevant error otherwise 
 */
SDK_STAT GetRefreshToken(Token* refreshToken);

/**
 * @brief write the refresh token to persistent storage
 * 
 * @param refreshToken 
 * @return SDK_SUCCESS upon success, relevant error otherwise 
 */
SDK_STAT UpdateRefreshToken(Token refreshToken);

/**
 * @brief called by the module which failed to verify using access token.
 * An asynchronous API to start the refresh process of the access token.
 * 
 * @return SDK_SUCCESS upon success, relevant error otherwise 
 */
SDK_STAT RefreshToken();

/**
 * @brief a checker API to test if access token is valid or not.
 * Function shall test vs. actual timestamp and update during refresh process.
 * 
 * @return true if token is valid
 * @return false if token is not valid
 */
bool IsAccessTokenValid();

#endif //_NETWORK_API_H_