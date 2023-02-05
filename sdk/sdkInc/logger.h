#ifndef _LOGGER_H
#define _LOGGER_H

#include "sdk_common.h"

#include <stdarg.h>

typedef enum{
    LOG_TYPE_DEBUG,
    LOG_TYPE_INFO,
    LOG_TYPE_WARNING,
    LOG_TYPE_ERROR,

    LOG_TYPE_DEBUG_INTERNAL,

    LOG_TYPE_NUM
} eLogTypes;

/**
 * @brief initialize the logger by creating queue for the upcoming logs.
 * 
 */
SDK_STAT LoggerInit();

/**
 * @brief Function to send logs only to the local logger and not to the server
 * 
 * @param message string of requested message.
 */
#ifdef DEBUG
#define LOG_DEBUG_INTERNAL(message, ...)      LogSend(LOG_TYPE_DEBUG_INTERNAL, message __VA_OPT__(,) __VA_ARGS__)
#else
#define LOG_DEBUG_INTERNAL(message, ...)  
#endif
/**
 * @brief Function to send debug sevirity type logs.
 * 
 * @param message string of requested message.
 */
#define LOG_DEBUG(message,...)      LogSend(LOG_TYPE_DEBUG, message __VA_OPT__(,) __VA_ARGS__)

/**
 * @brief Function to send info sevirity type logs.
 * 
 * @param message string of requested message.
 */
#define LOG_INFO(message,...)       LogSend(LOG_TYPE_INFO, messagev __VA_OPT__(,) __VA_ARGS__)

/**
 * @brief Function to send warning sevirity type logs.
 * 
 * @param message string of requested message.
 */
#define LOG_WARNING(message,...)    LogSend(LOG_TYPE_WARNING, message __VA_OPT__(,) __VA_ARGS__)

/**
 * @brief Function to send error sevirity type logs.
 * 
 * @param message string of requested message.
 */
#define LOG_ERROR(message,...)      LogSend(LOG_TYPE_ERROR, message __VA_OPT__(,) __VA_ARGS__)

/**
 * @brief Function to send logs.
 * 
 * @param logType enum of log sevirity type.
 * @param message string of requested message.
 */
void LogSend(eLogTypes logType, const char* message, ...);

#endif //_LOGGER_H