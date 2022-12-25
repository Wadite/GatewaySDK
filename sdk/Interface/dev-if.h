#ifndef _DEV_IF_H_
#define _DEV_IF_H_


#include "sdk_common.h"
#include "osal.h"

#include <stdint.h>
#include <stdbool.h>


/**
 * @brief DEV_IF - device interface. API to external devices over BLE or local bridge.
 * 
 */

/**
 * @brief device handle. One shall be used for all BLE bridges/nodes, and another one for
 * *each* local bridge.
 * 
 */
typedef void* dev_handle;

/**
 * @brief output of the logger, the output will be implemented by each platform, 
 * This is for local logs 
 * 
 */
typedef void* logger_local_handle;

/**
 * @brief callback to be called upon received device packet.
 * The callback must end shortly with no blocking activities.
 * 
 * @param dev device handle of previous initialized device
 * @param data complete advertise valid data
 * @param length the length of the advertise packet
 * @param rssi the rssi of the advertisement packet
 */
typedef void (*DevHandlePacketCB)(dev_handle dev, const void* data, uint32_t length, int32_t rssi);

/**
 * @brief Enum to enumerate devices
 * 
 */
typedef enum {
    DEV_BLE = 0,
    DEV_LOCAL_FIRST = 1, //first local bridge
    //TBD - what is max local bridge?
}DEV_ID;

/**
 * @brief start the BLE and/or local bridge activity.
 * After init and retrieving valid handle, device is ready to operate.
 * Init of DEV_BLE is expected to start BLE scan.
 * 
 * @param id which device ID to initialize.
 * @return dev_handle identifier to identify the specific device. NULL upon failure.
 */
dev_handle DevInit(DEV_ID id);

/**
 * @brief register a callback to handle uplink packets from devices
 * 
 * @param dev device handle of previous initialized device 
 * @param cb teh callback function to invoke
 * @return SDK_SUCCESS upon success, relevant error otherwise 
 */
SDK_STAT RegisterReceiveCallback(dev_handle dev, DevHandlePacketCB cb);

/**
 * @brief broadcast BLE advertise packet.
 * Data should be complete BLE Adv packet with no extra header.
 * Implementation layer may use the advertise, or in case of local bridge, ignore the duration and interval parameters.
 * 
 * @param dev device handle of previous initialized device
 * @param duration How long to keep sending the advertise packets, in ms.
 * @param interval Period to wait between transmissions in ms.
 * @param data complete advertise valid data
 * @param length the length of the advertise packet
 * @return SDK_SUCCESS upon success, relevant error otherwise 
 */

SDK_STAT DevSendPacket(dev_handle dev,
                       uint32_t duration,
                       uint32_t interval,
                       void* data,
                       uint32_t length);


/**
 * @brief API to check if the deice is busy or available for new packet send.
 * 
 * @param dev device handle of previous initialized device
 * @return true if device is busy in previous packet send
 * @return false if device is available for new packet send
 */
bool DevIsBusy(dev_handle dev);

/**
 * @brief Inits the logger local output (Can be UART or anything else)
 * @return logger_local_handle on success NULL on fail
 */
logger_local_handle DevLoggerInit();

/**
 * @brief Sends a packet to the local interface
 * @param data complete advertise valid data
 * @param length the length of the advertise packet
 * @return SDK_SUCCESS upon success
 * @return SDK_INVALID_PARAMS upon receiving NULL or length 0
 * @return SDK_FAILURE upon internal error
 */
SDK_STAT DevLoggerSend(logger_local_handle interface_handle, void* data, uint32_t length);

#endif //_DEV_IF_H_