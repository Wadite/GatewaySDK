#ifndef _HTTP_STRING_BUILDER_H
#define _HTTP_STRING_BUILDER_H

#include <stdint.h>

typedef enum{
    HTTP_MSG_TYPE_POST,
    HTTP_MSG_TYPE_PUT,

    HTTP_MSG_TYPE_NUM
}eHttpMsgType;

/**
 * @brief Build http post message.
 * 
 * @param urlExtenstion url extenstion of the msg.
 * @param headers pointer to buffer of headers.
 * @param headersSize number of headers.
 * @param body the body of the message
 * 
 * @return NULL if internal failt, otherwise the http ready string.
 */
char* BuildHttpPostMsg(char* urlExtenstion, const char** headers, uint16_t headersSize, const char* body);

/**
 * @brief Build http put message.
 * 
 * @param urlExtenstion url extenstion of the msg.
 * @param headers pointer to buffer of headers.
 * @param headersSize number of headers.
 * @param body the body of the message
 * 
 * @return NULL if internal failt, otherwise the http ready string.
 */
char* BuildHttpPutMsg(char* urlExtenstion, const char** headers, uint16_t headersSize, const char* body);
#endif //_HTTP_STRING_BUILDER_H