#ifndef _HTTP_STRING_BUILDER_H
#define _HTTP_STRING_BUILDER_H

#include <stdint.h>

char* BuildHttpPostMsg(char* urlExtenstion, const char** headers, uint16_t headersSize, const char* body);
char* BuildHttpPutMsg(char* urlExtenstion, const char** headers, uint16_t headersSize, const char* body);
#endif //_HTTP_STRING_BUILDER_H