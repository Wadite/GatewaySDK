#include "at-commands.h"
#include <assert.h>
#include <string.h>

#define FULL_MESSAGE_BUFFER_SIZE    (128)
#define COMMAND_PREFIX_SYMBOL       ('+')
#define COMMAND_READ_SYMBOL         ('?')
#define COMMAND_SEPERATE_SYMBOL     (',')
#define COMMAND_EXECUTE_SYMBOL      ('=')
#define COMMAND_STRING_SYMBOL       ('\"')
#define COMMNAD_END_OF_LINE         ("\r\n")
#define COMMAND_KEY_VALUE_SEPERATE  (':')

#define SIZE_OF_POST_PRE_RESPOND    (2)
#define RESPONSE_SIZE(size)         ((size) + SIZE_OF_POST_PRE_RESPOND)

static const char * s_atStr = "AT";

static const char * s_atCmdsTable[AT_CMD_NUM] = {
	[AT_CMD_TEST]       = "",
	[AT_CMD_QCCID]      = "QCCID",
	[AT_CMD_CTZR]       = "CTZR",
	[AT_CMD_CTZU]       = "CTZU",
    [AT_CMD_QCFG]       = "QCFG",
    [AT_CMD_CGDCONT]    = "CGDCONT",
	[AT_CMD_CEREG]      = "CEREG",
	[AT_CMD_COPS]       = "COPS",
    [AT_CMD_QCSQ]       = "QCSQ",
    [AT_CMD_QLTS]       = "QLTS",
    [AT_CMD_QPING]      = "QPING",
    [AT_CMD_QHTTPCFG]   = "QHTTPCFG",
    [AT_CMD_QHTTPURL]   = "QHTTPURL",
    [AT_CMD_QHTTPPOST]  = "QHTTPPOST",
    [AT_CMD_QHTTPGET]   = "QHTTPGET",
    [AT_CMD_QHTTPPUT]   = "QHTTPPUT",
    [AT_CMD_QHTTPREAD]  = "QHTTPREAD",
};

static const char * s_atQcfgExtConfgCmndTable[EXTENDED_CONFIGURE_NUM] = {
    [EXTENDED_CONFIGURE_RATS_SEARCHING_SEQUENCE]         = "\"nwscanseq\"",
    [EXTENDED_CONFIGURE_RATS_TO_BE_SEARCHED]             = "\"nwscanmode\"",
    [EXTENDED_CONFIGURE_NETWORK_SEARCH_UNDER_LTE_RAT]    = "\"iotopmode\"",
};

static const char * s_atCgdcontPdpTypeTable[PDP_TYPE_NUM] = {
    [PDP_TYPE_IPV4]     = "\"IPV4\"",
    [PDP_TYPE_PPP]      = "\"PPP\"",
    [PDP_TYPE_IPV6]     = "\"IPV6\"",
    [PDP_TYPE_IPV4V6]   = "\"IPV4V6\"",
};

static const char * s_atQhttpcfgCmndTable[HTTP_CONFIGURATION_NUM] = {
    [HTTP_CONFIGURATION_PDP_CONTEXT_ID]     = "\"contextid\"",
    [HTTP_CONFIGURATION_REQUEST_HEADER]     = "\"requestheader\"",
    [HTTP_CONFIGURATION_RESPOSNSE_HEADER]   = "\"responseheader\"",
    [HTTP_CONFIGURATION_CONTENT_TYPE]       = "\"contenttype\""
};

static uint8_t bufferForTesting[FULL_MESSAGE_BUFFER_SIZE];

static inline int writeCtzr(const AtCmndsParams* cmndParams, int offset)
{
    return sprintf((bufferForTesting + offset), "%d", cmndParams->atCtzrParams.mode);
}

static inline int writeCtzu(const AtCmndsParams* cmndParams, int offset)
{
    return sprintf((bufferForTesting + offset), "%d", cmndParams->atCtzuParams.mode);
}

static inline int writeQcfg(const AtCmndsParams * cmndParams, int * offset)
{
    (*offset) += sprintf((bufferForTesting + (*offset)), "%s", s_atQcfgExtConfgCmndTable[cmndParams->atQcfgParams.cmd]);
    (*offset) += sprintf((bufferForTesting + (*offset)), "%c", COMMAND_SEPERATE_SYMBOL);
    (*offset) += sprintf((bufferForTesting + (*offset)), "%s", cmndParams->atQcfgParams.mode);
    (*offset) += sprintf((bufferForTesting + (*offset)), "%c", COMMAND_SEPERATE_SYMBOL);
    return sprintf((bufferForTesting + (*offset)), "%d", cmndParams->atQcfgParams.effect);
}

static inline int writeCgdcont(const AtCmndsParams * cmndParams, int * offset)
{
    (*offset) += sprintf((bufferForTesting + (*offset)), "%d", cmndParams->atCgdcontParams.cid);
    (*offset) += sprintf((bufferForTesting + (*offset)), "%c", COMMAND_SEPERATE_SYMBOL);
    (*offset) += sprintf((bufferForTesting + (*offset)), "%s", s_atCgdcontPdpTypeTable[cmndParams->atCgdcontParams.type]);
    (*offset) += sprintf((bufferForTesting + (*offset)), "%c", COMMAND_SEPERATE_SYMBOL);
    (*offset) += sprintf((bufferForTesting + (*offset)), "%c", COMMAND_STRING_SYMBOL);
    (*offset) += sprintf((bufferForTesting + (*offset)), "%s", cmndParams->atCgdcontParams.apn);
    return sprintf((bufferForTesting + (*offset)), "%c", COMMAND_STRING_SYMBOL);
}

static inline int writeCereg(const AtCmndsParams * cmndParams, int offset)
{
    return sprintf((bufferForTesting + offset), "%d", cmndParams->atCereg.status);
}

static inline int writeQlts(const AtCmndsParams * cmndParams, int offset)
{
    return sprintf((bufferForTesting + offset), "%d", cmndParams->atQlts.mode);
}

static inline int writeQping(const AtCmndsParams * cmndParams, int * offset)
{
    (*offset) += sprintf((bufferForTesting + (*offset)), "%d", cmndParams->atQping.contextId);
    (*offset) += sprintf((bufferForTesting + (*offset)), "%c", COMMAND_SEPERATE_SYMBOL);
    (*offset) += sprintf((bufferForTesting + (*offset)), "%c", COMMAND_STRING_SYMBOL);
    (*offset) += sprintf((bufferForTesting + (*offset)), "%s", cmndParams->atQping.host);
    return sprintf((bufferForTesting + (*offset)), "%c", COMMAND_STRING_SYMBOL);
}

static inline int writeQhttpcfg(const AtCmndsParams * cmndParams, int * offset)
{
    (*offset) += sprintf((bufferForTesting + (*offset)), "%s", s_atQhttpcfgCmndTable[cmndParams->atQhttpcfg.cmd]);
    (*offset) += sprintf((bufferForTesting + (*offset)), "%c", COMMAND_SEPERATE_SYMBOL);
    return sprintf((bufferForTesting + (*offset)), "%d", cmndParams->atQhttpcfg.value);
}

static inline int writeQhttpurl(const AtCmndsParams * cmndParams, int * offset)
{
    (*offset) += sprintf((bufferForTesting + (*offset)), "%d", cmndParams->atQhttpurl.urlLength);
    (*offset) += sprintf((bufferForTesting + (*offset)), "%c", COMMAND_SEPERATE_SYMBOL);
    return sprintf((bufferForTesting + (*offset)), "%d", cmndParams->atQhttpurl.urlTimeout);
}

static inline int writeQhttppost(const AtCmndsParams * cmndParams, int * offset)
{
    (*offset) += sprintf((bufferForTesting + (*offset)), "%d", cmndParams->atQhttppost.postBodySize);
    (*offset) += sprintf((bufferForTesting + (*offset)), "%c", COMMAND_SEPERATE_SYMBOL);
    (*offset) += sprintf((bufferForTesting + (*offset)), "%d", cmndParams->atQhttppost.inputTimeout);
    (*offset) += sprintf((bufferForTesting + (*offset)), "%c", COMMAND_SEPERATE_SYMBOL);
    return sprintf((bufferForTesting + (*offset)), "%d", cmndParams->atQhttppost.responseTimeout);
}

static inline int writeQhttpget(const AtCmndsParams * cmndParams, int offset)
{
    return sprintf((bufferForTesting + offset), "%d", cmndParams->atQhttpurl.urlTimeout);
}

static inline int writeQhttread(const AtCmndsParams * cmndParams, int offset)
{
    return sprintf((bufferForTesting + offset), "%d", cmndParams->atQhttpurl.urlTimeout);
}

SDK_STAT AtTestCmd()
{
    // For now not implemeted because no test commands, always fails
    // TODO: implement
    return SDK_FAILURE;
}

SDK_STAT AtReadCmd(const eAtCmds atCmd)
{
    int offset = 0;

    if(!(0 <= atCmd && atCmd < AT_CMD_NUM))
    {
        return SDK_INVALID_PARAMS;
    }
    assert(s_atCmdsTable[atCmd]);

    offset += sprintf((bufferForTesting + offset), "%s", s_atStr);
    offset += sprintf((bufferForTesting + offset), "%c", COMMAND_PREFIX_SYMBOL);
    offset += sprintf((bufferForTesting + offset), "%s", s_atCmdsTable[atCmd]);
    offset += sprintf((bufferForTesting + offset), "%c", COMMAND_READ_SYMBOL);
    offset += sprintf((bufferForTesting + offset), "%s", COMMNAD_END_OF_LINE);

    return ModemSend(bufferForTesting, offset);
}

SDK_STAT AtWriteCmd(const eAtCmds atCmd, const AtCmndsParams * cmndParams)
{
    int offset = 0;

    if(!(0 <= atCmd && atCmd < AT_CMD_NUM) || !cmndParams)
    {
      return SDK_INVALID_PARAMS;  
    }
    assert(s_atCmdsTable[atCmd]);

    offset += sprintf((bufferForTesting + offset), "%s", s_atStr);
    offset += sprintf((bufferForTesting + offset), "%c", COMMAND_PREFIX_SYMBOL);
    offset += sprintf((bufferForTesting + offset), "%s", s_atCmdsTable[atCmd]);
    offset += sprintf((bufferForTesting + offset), "%c", COMMAND_EXECUTE_SYMBOL);

    switch(atCmd)
    {
        case AT_CMD_CTZR:
            offset += writeCtzr(cmndParams, offset);
            break;

        case AT_CMD_CTZU:
            offset += writeCtzu(cmndParams, offset);
            break;

        case AT_CMD_QCFG:
            offset += writeQcfg(cmndParams, &offset);
            break;

        case AT_CMD_CGDCONT:
            offset += writeCgdcont(cmndParams, &offset);
            break;

        case AT_CMD_CEREG:
            offset += writeCereg(cmndParams, offset);
            break;

        case AT_CMD_QLTS:
            offset += writeQlts(cmndParams, offset);
            break;

        case AT_CMD_QPING:
            offset += writeQping(cmndParams, &offset);
            break;

        case AT_CMD_QHTTPCFG:
            offset += writeQhttpcfg(cmndParams, &offset);
            break;

        case AT_CMD_QHTTPURL:
            offset += writeQhttpurl(cmndParams, &offset);
            break;

        case AT_CMD_QHTTPPOST:
            offset += writeQhttppost(cmndParams, &offset);
            break;

        case AT_CMD_QHTTPGET:
            offset += writeQhttpget(cmndParams, offset);
            break;

        case AT_CMD_QHTTPPUT:
            offset += writeQhttppost(cmndParams, &offset);
        break;

        case AT_CMD_QHTTPREAD:
            offset += writeQhttread(cmndParams, offset);
            break;       

        default:
            return SDK_INVALID_PARAMS;  
    }

    offset += sprintf((bufferForTesting + offset), "%s", COMMNAD_END_OF_LINE);

    return ModemSend(bufferForTesting, offset);
}

SDK_STAT AtExecuteCmd(const eAtCmds atCmd)
{
    int offset = 0;

    if(!(0 <= atCmd && atCmd < AT_CMD_NUM))
    {
        return SDK_INVALID_PARAMS;
    }
    assert(s_atCmdsTable[atCmd]);

    offset += sprintf((bufferForTesting + offset), "%s", s_atStr);
    if(atCmd != AT_CMD_TEST)
    {
        offset += sprintf((bufferForTesting + offset), "%c", COMMAND_PREFIX_SYMBOL);
        offset += sprintf((bufferForTesting + offset), "%s", s_atCmdsTable[atCmd]);
    }
    offset += sprintf((bufferForTesting + offset), "%s", COMMNAD_END_OF_LINE);

    return ModemSend(bufferForTesting, offset);
}

SDK_STAT atEnumToResponseString(eAtCmds atCmd, char * responseBuffer, uint16_t bufferSize)
{
    assert(s_atCmdsTable[atCmd]);
    if(atCmd >= AT_CMD_NUM || !responseBuffer || (bufferSize == 0))
    {
        return SDK_INVALID_PARAMS;
    }

    uint16_t stringSize = strlen(s_atCmdsTable[atCmd]);
    uint16_t index = 0;

    if(bufferSize <= RESPONSE_SIZE(stringSize))
    {
        return SDK_INVALID_PARAMS;
    }

    responseBuffer[index++] = COMMAND_PREFIX_SYMBOL;
    index += sprintf((responseBuffer + index), "%s",s_atCmdsTable[atCmd]);
    responseBuffer[index++] = COMMAND_KEY_VALUE_SEPERATE;
    responseBuffer[index++] = '\0';

    return SDK_SUCCESS;

}