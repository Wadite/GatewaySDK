#include <stdio.h>
#include <assert.h>
#include "httpStringBuilder.h"

#define SIZE_OF_HTTP_MSG        (2048)

#define STR_SPACE               (' ')
#define STR_CRLF                ("\r\n")
#define STR_END                 ('\0')
#define HTTP_VER                ("HTTP/1.1")

typedef enum{
    HTTP_MSG_TYPE_POST,
    HTTP_MSG_TYPE_PUT,

    HTTP_MSG_TYPE_NUM
}eHttpMsgType;

static const char * s_httpMsgTypetable[HTTP_MSG_TYPE_NUM] = {
    [HTTP_MSG_TYPE_POST]         = "POST",
    [HTTP_MSG_TYPE_PUT]          = "PUT",
};

static char* buildHttpMsg(eHttpMsgType httpMsgType, char* urlExtenstion,const char** headers, uint16_t headersSize, const char* body)
{
    if(httpMsgType >= HTTP_MSG_TYPE_NUM)
    {
        return NULL;
    }

    static char s_httpMsg[SIZE_OF_HTTP_MSG] = {0};
    int index = 0;

    index += sprintf((s_httpMsg + index),"%s",s_httpMsgTypetable[httpMsgType]);
    index += sprintf((s_httpMsg + index),"%c",STR_SPACE);

    if(urlExtenstion)
    {
        index += sprintf((s_httpMsg + index),"%s",urlExtenstion);
        index += sprintf((s_httpMsg + index),"%c",STR_SPACE);
    }

    index += sprintf((s_httpMsg + index),HTTP_VER);
    index += sprintf((s_httpMsg + index),STR_CRLF);

    if(headersSize)
    {
        assert(headers);
        for(uint16_t i = 0;i < headersSize; i++)
        {
            assert(headers[i]);
            index += sprintf((s_httpMsg + index),"%s",headers[i]);
            index += sprintf((s_httpMsg + index),STR_CRLF);
        }
    }

    index += sprintf((s_httpMsg + index),STR_CRLF);

    if(body)
    {
        index += sprintf((s_httpMsg + index),"%s",body);
        index += sprintf((s_httpMsg + index),STR_CRLF); 
    }

    index += sprintf((s_httpMsg + index),"%c",STR_END);

    return s_httpMsg;

}

char* BuildHttpPostMsg(char* urlExtenstion, const char** headers, uint16_t headersSize, const char* body)
{
    return buildHttpMsg(HTTP_MSG_TYPE_POST, urlExtenstion, headers, headersSize, body);
}

char* BuildHttpPutMsg(char* urlExtenstion, const char** headers, uint16_t headersSize, const char* body)
{
    return buildHttpMsg(HTTP_MSG_TYPE_PUT, urlExtenstion, headers, headersSize, body);
}
