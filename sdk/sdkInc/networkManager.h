#ifndef _NETWORK_MANAGER_H_
#define _NETWORK_MANAGER_H_

SDK_STAT NetworkManagerInit();

SDK_STAT NetworkMqttMsgSend(const char* topic, void* pkt, uint32_t length);

#endif //_NETWORK_MANAGER_H_