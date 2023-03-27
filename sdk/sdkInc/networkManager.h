#ifndef _NETWORK_MANAGER_H_
#define _NETWORK_MANAGER_H_

typedef enum{
	CONN_STATE_DISCONNECTION,
    CONN_STATE_CONNECTION,

    CONN_STATE_NUM
} eConnectionStates;

/**
 * @brief Initialize the network manager module.
 * 
 * @return SDK_FAILURE if queue or mutex creation failed.
 * @return SDK_SUCCESS if initialized successfully, other received internal error.
 */
SDK_STAT NetworkManagerInit();

/**
 * @brief Send mqtt msg to server.

 * @param topic the name of the topic to send.
 * @param pkt the data to be send.
 * @param length size of the data.
 * 
 * @return SDK_SUCCESS if send successfully, other received internal error.
 */
SDK_STAT NetworkMqttMsgSend(const char* topic, void* pkt, uint32_t length);

#endif //_NETWORK_MANAGER_H_