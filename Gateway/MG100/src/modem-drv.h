#ifndef _MODEM_DRV_H
#define _MODEM_DRV_H

#include <stdint.h>
#include <sdk_common.h>

/**
 * @brief Function pointer for modem receive buffer received will have a '\0' at the end
 * 
 */
typedef void (*ReadModemDataCB)(uint8_t* buffer, uint16_t size);

/**
 * @brief initialize the modem message send via uart, create thread for message comming from modem.
 * 
 */
SDK_STAT ModemInit(ReadModemDataCB cb);

/**
 * @brief API to send message via uart to modem.
 * 
 * @param dataBuff pointer to buffer of message data.
 * @param buffSize size of data message buffer.
 * 
 * @return SDK_INVALID_PARAMS if bad parameters were passed.
 * @return SDK_FAILURE if internal bug occurred(higher level should assert).
 * @return SDK_SUCCESS if messaged was sent succefully.
 */
SDK_STAT ModemSend(const void* dataBuff, uint16_t buffSize);

#endif //_MODEM_DRV_H