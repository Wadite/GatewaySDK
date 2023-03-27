#include "network-api.h"
#include "at-commands.h"
#include "modem-drv.h"
#include "cJSON.h"
#include "httpStringBuilder.h"
#include "williotSdkJson.h"
#include "osal.h"
#include "MQTTPacket.h"
#include "sdkConfigurations.h"
#include "logger.h"
#include "flash-drv.h"
#include "sdkUtils.h"

#include <zephyr/zephyr.h>
#include <assert.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define STR_HELPER(x)						#x
#define INT_TO_STR(x) 						STR_HELPER(x)

#define SIZE_OF_COMPARE_STRING				(128)
#define MODEM_WAKE_TIMEOUT					(40000)/*30000*/
#define AT_CMD_TIMEOUT						(1000)
#define RESPONSE_EVENT_BIT_SET				(0x001)

#define IS_JSON_PREFIX(buff)			  	(*(buff) == '{')
#define IS_WAITED_WORD(word,cmpWord,size)	(memcmp((word), (cmpWord), (size)) == 0)
#define IS_NO_REFRESH_TOKEN(buff,zeros)		(memcmp((buff),(zeros),sizeof((buff))) == 0)

#define SEARCHING_SEQUENCE					"020202"
#define TAKE_EFFECT_IMMEDIATELY				(1)
#define SCAN_MODE_GSM_ONLY					"0"
#define NETWORK_CATEGORY_EMTC				"0"
#define PDP_CONTEXT_IDENTIFIER				(1)
#define ACCESS_POINT_NAME					"uinternet"

#define NETWORK_AVAILABLE_CONTEX_ID 		(1)
#define ADDRESS_TO_PING 					"www.google.com"
#define MODEM_RESPONSE_VERIFY_WORD			"OK"
#define IMEI_LEN                            (15)
#define MODEM_HTTP_URL_VERIFY_WORD			"CONNECT"
#define MODEM_READY_VERITY_WORD				"APP RDY"
#define MODEM_CEREG_SUCCESS_PAYLOAD			" 1,1"
#define TIME_TO_RETRY_CEREG_PAYLOAD			(10000)

#define RESPONSE_BUFFER_SIZE				(50)
#define API_SEC_KEY						    "MTRlNzAwMjItZTNiNi00ODZjLThkOTQtM2ZmZTk3MTEyM2NjOlFxMXR3THlvRzNSbkRiTm51ZVV3Uk9BeElSUlVxa3pJV0RrLW55NmZLQ3c="
#define CONNECTION_TOKEN_TYPE				"Bearer"
#define HTTP_SEND_RECEIVE_TIMEOUT			(80)
#define SIZE_OF_ACCESS_CONNECTION_TOKEN		(1251)
#define SIZE_OF_DEVICE_CODE					(43)
#define SIZE_OF_USER_CODE					(6)
#define HTTP_CONFIG_CONTEX_ID				(1)
#define HTTP_CONFIG_REQUEST_HEADER			(1)
#define SIZE_OF_ACCESS_TOKEN_FIRST			(929)
#define SIZE_OF_ACCESS_TOKEN_FULL			(SIZE_OF_ACCESS_TOKEN_FIRST + 21)
#define SIZE_OF_REFRESH_TOKEN				(54)		
#define TIME_TO_RETRY_TOKEN_RECEIVE			(5000)
#define SIZE_OF_RESPONSE_PAYLOAD			(20)
#define SIZE_OF_ADDRESS_BUFFER				(256)
#define CONNECTION_HANDLE					(1)

#define HEADER_SEPERATE					    ": "
#define HEADER_HOST							"Host"
#define HEADER_AUTHORIZATION				"Authorization"
#define HEADER_CONTENT_LENGTH				"Content-Length"
#define HEADER_CONTENT_TYPE					"Content-Type"

#define SHORT_URL 							"api.wiliot.com"
#define CONNECTION_URL						"https://"SHORT_URL
#define TYPE_APP_JSON						"application/json"
#define URL_EXT_ACCESS_TOKEN				"/v1/auth/token/api"
#define URL_EXT_GATEWAY_OWN(buff)			getUrlExtGateWayOwn((buff))
#define URL_EXT_DEVICE_AUTH					"/v1/gateway/device-authorize"
#define URL_EXT_REG_GATEWAY					"/v2/gateway/registry"
#define URL_EXT_GET_TOKENS					"/v2/gateway/token"
#define URL_EXT_UPDATE_ACCESS				"/v1/gateway/refresh?refresh_token="
#define URL_PRE_REGISTER                    "/v1/owner/" 

#define MAX_BODY_SIZE_DIGITS_NUM			(5)
#define BODY_GATEWAY(buff)					getBodyGateWay((buff))
#define BODY_REG_USR_CODE_PRE(buff)			getBodyRegUsrCodePre((buff))
#define BODY_REG_DEV_CODE_PRE(buff)			getBodyRegDevCodePre((buff))
#define BODY_REG_POST						"\"\r\n}"

#define MQTT_MAX_PACKET_SIZE				(1024)
#define MQTT_PDP_CTX_ID						(1)
#define MQTT_SSL_CTX_ID						(0)
#define MQTT_CLIENT_IDX_INTERNAL			0		// This is on purpose
#define MQTT_CLIENT_IDX						(MQTT_CLIENT_IDX_INTERNAL)
#define MQTT_SERV_PORT						(8883)
#define MQTT_INPUT_STRING					"> "
#define MQTT_BROKER_RESPONSE_STRING			"+QSSLURC: \"recv\"," INT_TO_STR(MQTT_CLIENT_IDX_INTERNAL)
#define MQTT_BROKER_CLOSED_STRING			"+QSSLURC: \"closed\"," INT_TO_STR(MQTT_CLIENT_IDX_INTERNAL)
#define MQTT_GOOD_PUBLISH_RESPOSE			"SEND OK"
#define MQTT_RECEIVE_LENGTH					(1500)
#define MQTT_KEEP_ALIVE_INTERVAL			(60)
#define MQTT_CLEAN_SESSION					(1)
#define MQTT_CONNECT_FOOTER_CHAR			(32)
#define MQTT_SUBSCRIBE_FOOTER_CHAR			(144)
#define MQTT_PACKET_DEFAULT_DUP				(0)
#define MQTT_PACKET_DEFAULT_QOS				(0)
#define MQTT_PACKET_DEFAULT_FLAG			(0)
#define MQTT_PUBLISH_TIME_RESPONSE			(1000)
#define MQTT_ERROR_CODE_CONNECT				{32,2,0,0}
#define MQTT_ERROR_CODE_SUBSCRIBE			{144,3,0,1,0}

typedef struct{
	bool isWaitingResponse;
	char * cmprWord;
	size_t wordSize;
} CallbackStruct;

static struct k_event s_responseEvent;
static CallbackStruct s_callbackStruct;
static cJSON *s_lastJson = NULL;
static char s_accessToken[SIZE_OF_ACCESS_TOKEN_FULL + 1] = {0};
static char s_refreshToken[SIZE_OF_REFRESH_TOKEN + 1] = {0};
static char s_responseExtraPayload[SIZE_OF_RESPONSE_PAYLOAD + 1] = {0};
static bool s_isLTEConfigurationDone = false;
static unsigned char s_mqttPacketBuff[MQTT_MAX_PACKET_SIZE] = {0};
static char s_addressStringBuffer[SIZE_OF_ADDRESS_BUFFER] = {0};
NetMQTTPacketCB s_receivedMQTTPacketHandle = 0;
handleConnStateChangeCB s_receivedConnStateHandle = 0;
static conn_handle s_connHandle = 0;
static bool s_anyResponse = false;

static char * getUrlExtGateWayOwn(char * buff) 
{
	SDK_STAT status = SDK_SUCCESS;
	int offset = 0;
	const char * accountIdPtr = NULL;

	offset = sprintf(buff, URL_PRE_REGISTER);

	status = GetAccountID(&accountIdPtr);
	assert(status == SDK_SUCCESS);

    offset += sprintf(buff + offset, "%s", accountIdPtr);
    offset += sprintf(buff + offset, "/gateway");

    return buff; 
}

static void getBodyGateWay(char * buff) 
{
	SDK_STAT status = SDK_SUCCESS;
	int offset = 0;
	const char * gateWayPtr = NULL;

	offset = sprintf(buff,"{\r\n    \"gateways\":[\"");

	status = GetGatewayId(&gateWayPtr);
	assert(status == SDK_SUCCESS);

    offset += sprintf(buff + offset, "%s", gateWayPtr);
    offset += sprintf(buff + offset, "\"]\r\n}");
}

static char * getBodyRegUsrCodePre(char * buff) 
{
	SDK_STAT status = SDK_SUCCESS;
	int offset = 0;
	const char * gateWayPtr = NULL;

	offset = sprintf(buff,"{\r\n    \"gatewayId\":\"");

	status = GetGatewayId(&gateWayPtr);
	assert(status == SDK_SUCCESS);

    offset += sprintf(buff + offset, "%s", gateWayPtr);
    offset += sprintf(buff + offset, "\",\r\n    \"userCode\": \"");

    return buff; 
}

static char * getBodyRegDevCodePre(char * buff) 
{
	SDK_STAT status = SDK_SUCCESS;
	int offset = 0;
	const char * gateWayPtr = NULL;

	offset = sprintf(buff,"{\r\n    \"gatewayId\":\"");

	status = GetGatewayId(&gateWayPtr);
	assert(status == SDK_SUCCESS);

    offset += sprintf(buff + offset, "%s", gateWayPtr);
    offset += sprintf(buff + offset, "\",\r\n    \"deviceCode\": \"");

    return buff; 
}

static void connectionStateChangeProcess()
{
	s_receivedConnStateHandle(s_connHandle);
}

static void postConnectionMessageProcess(uint8_t* buff, uint16_t buffSize)
{
	static bool incommingServerMsg = false;

	if(strncmp(MQTT_BROKER_RESPONSE_STRING, buff, strlen(MQTT_BROKER_RESPONSE_STRING)) == 0)
	{
		incommingServerMsg = true;
	}
	else if(strncmp(MQTT_BROKER_CLOSED_STRING, buff, strlen(MQTT_BROKER_CLOSED_STRING)) == 0)
	{
		s_isLTEConfigurationDone = false;
		connectionStateChangeProcess();
	}
	else if(incommingServerMsg && s_receivedMQTTPacketHandle)
	{
		s_receivedMQTTPacketHandle(buff,buffSize);
		incommingServerMsg = false;
	}
}

static void modemReadCallback(uint8_t* buff, uint16_t buffSize)
{	
	if(s_isLTEConfigurationDone)
	{
		postConnectionMessageProcess(buff, buffSize);
	}

	else
	{
		// Verify if received Json
		if(IS_JSON_PREFIX(buff))
		{
			char * cJsonString = NULL;
			// Don't forget to free 
			s_lastJson = cJSON_Parse(buff);

			cJsonString = cJSON_Print(s_lastJson);
            LOG_DEBUG_INTERNAL("%s\r\n",cJsonString);
			printk("%s\r\n",cJsonString);
			FreeJsonString(cJsonString);
		}
		else
		{
			LOG_DEBUG_INTERNAL("%s\r\n",buff);
			printk("%s\r\n",buff);
		}
	}

	if(IS_WAITED_WORD(buff,s_callbackStruct.cmprWord, s_callbackStruct.wordSize))
	{
		if((buffSize > s_callbackStruct.wordSize) && ((buffSize - s_callbackStruct.wordSize) < SIZE_OF_RESPONSE_PAYLOAD))
		{	
			sprintf(s_responseExtraPayload, "%s", (buff + s_callbackStruct.wordSize));
		}
		k_event_post(&s_responseEvent, RESPONSE_EVENT_BIT_SET);
	}
    else if(true == s_anyResponse)
    {
		if((buffSize >= IMEI_LEN) && (buffSize < SIZE_OF_RESPONSE_PAYLOAD))
		{	
			sprintf(s_responseExtraPayload, "%s", buff);
            s_anyResponse = false;
		}
		k_event_post(&s_responseEvent, RESPONSE_EVENT_BIT_SET);
    }
}

static inline SDK_STAT modemResponseWait(char * waitWord, size_t wordSize, uint32_t timeToWait)
{
	uint32_t waitEventState = 0;

	if(!waitWord || wordSize == 0)
	{
		return SDK_INVALID_PARAMS;
	}

	s_callbackStruct.cmprWord = waitWord;
	s_callbackStruct.wordSize = wordSize;
	waitEventState = k_event_wait(&s_responseEvent, RESPONSE_EVENT_BIT_SET, true, K_MSEC(timeToWait));

	if(waitEventState == 0)
	{
		return SDK_TIMEOUT;
	}

	return SDK_SUCCESS;
}

static SDK_STAT setConnectionUrl(void)
{
	SDK_STAT status = SDK_SUCCESS;
	AtCmndsParams cmndsParams = {0};

	cmndsParams.atQhttpcfg.cmd = HTTP_CONFIGURATION_PDP_CONTEXT_ID;
	cmndsParams.atQhttpcfg.value = HTTP_CONFIG_CONTEX_ID;
	status = AtWriteCmd(AT_CMD_QHTTPCFG, &cmndsParams);
	__ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	status = modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	cmndsParams.atQhttpcfg.cmd = HTTP_CONFIGURATION_REQUEST_HEADER;
	cmndsParams.atQhttpcfg.value = HTTP_CONFIG_REQUEST_HEADER;
	status = AtWriteCmd(AT_CMD_QHTTPCFG, &cmndsParams);
	__ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	status = modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	cmndsParams.atQhttpurl.urlLength = strlen(CONNECTION_URL);
	cmndsParams.atQhttpurl.urlTimeout = HTTP_SEND_RECEIVE_TIMEOUT;
	status = AtWriteCmd(AT_CMD_QHTTPURL, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	status = modemResponseWait(MODEM_HTTP_URL_VERIFY_WORD, strlen(MODEM_HTTP_URL_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);
	ModemSend(CONNECTION_URL, cmndsParams.atQhttpurl.urlLength);
	status = modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	return SDK_SUCCESS;
}

static SDK_STAT sentAndReadHttpMsg(AtCmndsParams cmndsParams, eAtCmds atCmd, char* httpMsgString)
{
	SDK_STAT status = SDK_SUCCESS;
	char responseBuffer[RESPONSE_BUFFER_SIZE] = {0};

	__ASSERT(atCmd == AT_CMD_QHTTPPOST || atCmd == AT_CMD_QHTTPPUT,"sentAndReadHttpMsg not http cmd");
	status = AtWriteCmd(atCmd, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	status = modemResponseWait(MODEM_HTTP_URL_VERIFY_WORD, strlen(MODEM_HTTP_URL_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
    if (status != SDK_SUCCESS)
    {
        printk("qhttp put/post %d\n", status);
        OsalSleep(MODEM_WAKE_TIMEOUT);
        status = AtWriteCmd(atCmd, &cmndsParams);
        status = modemResponseWait(MODEM_HTTP_URL_VERIFY_WORD, strlen(MODEM_HTTP_URL_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
        printk("qhttp put/post 2nd attempt %d\n", status);
    }
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);
	ModemSend(httpMsgString, strlen(httpMsgString));
	status = modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
    printk("httpmsgstring %d\n", status);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);
	status = atEnumToResponseString(atCmd, responseBuffer, RESPONSE_BUFFER_SIZE);
	__ASSERT((status == SDK_SUCCESS),"atEnumToResponseString internal fail");
	status = modemResponseWait(responseBuffer, strlen(responseBuffer), MODEM_WAKE_TIMEOUT);
    printk("responsegetting %d\n", status);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	status = AtWriteCmd(AT_CMD_QHTTPREAD, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	status = atEnumToResponseString(AT_CMD_QHTTPREAD, responseBuffer, RESPONSE_BUFFER_SIZE);
	__ASSERT((status == SDK_SUCCESS),"atEnumToResponseString internal fail");
	status = modemResponseWait(responseBuffer, strlen(responseBuffer), MODEM_WAKE_TIMEOUT);
    printk("qhttpread %d\n", status);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	return SDK_SUCCESS;
}

static SDK_STAT getConnectionAccessToken(char * accessConnectionToken)
{
	AtCmndsParams cmndsParams = {0};
	char * httpMsgString = NULL;

	const char* headers[] ={
		HEADER_HOST HEADER_SEPERATE SHORT_URL,
		HEADER_AUTHORIZATION HEADER_SEPERATE API_SEC_KEY
	};

	uint16_t headersSize = sizeof(headers)/sizeof(headers[0]);

	httpMsgString = BuildHttpPostMsg(URL_EXT_ACCESS_TOKEN, headers, headersSize, NULL);
	cmndsParams.atQhttppost.postBodySize = strlen(httpMsgString);
	cmndsParams.atQhttppost.inputTimeout = HTTP_SEND_RECEIVE_TIMEOUT;
	cmndsParams.atQhttppost.responseTimeout = HTTP_SEND_RECEIVE_TIMEOUT;

	sentAndReadHttpMsg(cmndsParams, AT_CMD_QHTTPPOST, httpMsgString);

	cJSON *accessTokenJson = cJSON_GetObjectItem(s_lastJson, "access_token");
	memcpy(accessConnectionToken, cJSON_GetStringValue(accessTokenJson), SIZE_OF_ACCESS_CONNECTION_TOKEN);
	cJSON_Delete(s_lastJson);

	return SDK_SUCCESS;
}

static SDK_STAT setConnectionGateway(char * authorizationHeader)
{
	SDK_STAT status = SDK_SUCCESS;
	AtCmndsParams cmndsParams = {0};
	char * httpMsgString = NULL;
	int index = 0;
	char bodyGateWay[SIZE_OF_ADDRESS_BUFFER] = {0};

	BODY_GATEWAY(bodyGateWay);
	char contLengthHeader[sizeof(HEADER_CONTENT_LENGTH) + sizeof(HEADER_SEPERATE) + MAX_BODY_SIZE_DIGITS_NUM]; 
	index += sprintf((contLengthHeader + index),HEADER_CONTENT_LENGTH HEADER_SEPERATE);
	index += sprintf((contLengthHeader + index),"%d",strlen(bodyGateWay));

	const char* headers[] ={
		HEADER_HOST HEADER_SEPERATE SHORT_URL,
		contLengthHeader,
		authorizationHeader,
		HEADER_CONTENT_TYPE HEADER_SEPERATE TYPE_APP_JSON
	};

	uint16_t headersSize = sizeof(headers)/sizeof(headers[0]);

	httpMsgString = BuildHttpPutMsg(URL_EXT_GATEWAY_OWN(s_addressStringBuffer), headers, headersSize, bodyGateWay);
	cmndsParams.atQhttppost.postBodySize = strlen(httpMsgString);
	cmndsParams.atQhttppost.inputTimeout = HTTP_SEND_RECEIVE_TIMEOUT;
	cmndsParams.atQhttppost.responseTimeout = HTTP_SEND_RECEIVE_TIMEOUT;

	status = sentAndReadHttpMsg(cmndsParams, AT_CMD_QHTTPPUT, httpMsgString);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	cJSON *checkDataJson = cJSON_GetObjectItem(s_lastJson, "data");
	LOG_DEBUG_INTERNAL("Json response: %s\r\n",cJSON_GetStringValue(checkDataJson));
	cJSON_Delete(s_lastJson);

	return SDK_SUCCESS;
}

static SDK_STAT getConnectionCodes(char * userCode, char * deviceCode)
{
	SDK_STAT status = SDK_SUCCESS;
	AtCmndsParams cmndsParams = {0};
	char * httpMsgString = NULL;

	const char* headers[] ={
		HEADER_HOST HEADER_SEPERATE SHORT_URL
	};

	uint16_t headersSize = sizeof(headers)/sizeof(headers[0]);

	httpMsgString = BuildHttpPostMsg(URL_EXT_DEVICE_AUTH, headers, headersSize, NULL);
	cmndsParams.atQhttppost.postBodySize = strlen(httpMsgString);
	cmndsParams.atQhttppost.inputTimeout = HTTP_SEND_RECEIVE_TIMEOUT;
	cmndsParams.atQhttppost.responseTimeout = HTTP_SEND_RECEIVE_TIMEOUT;

	status = sentAndReadHttpMsg(cmndsParams, AT_CMD_QHTTPPOST, httpMsgString);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	cJSON * checkDataJson = cJSON_GetObjectItem(s_lastJson, "device_code");
	memcpy(deviceCode, cJSON_GetStringValue(checkDataJson), SIZE_OF_DEVICE_CODE);
	checkDataJson = cJSON_GetObjectItem(s_lastJson, "user_code");
	memcpy(userCode, cJSON_GetStringValue(checkDataJson), SIZE_OF_USER_CODE);
	cJSON_Delete(s_lastJson);

	return SDK_SUCCESS;
}

static SDK_STAT registerConnectionGateway(char * bodyUserCode)
{
	SDK_STAT status  = SDK_SUCCESS;
	AtCmndsParams cmndsParams = {0};
	char * httpMsgString = NULL;
	int index = 0;

	char contLengthHeader[sizeof(HEADER_CONTENT_LENGTH) + sizeof(HEADER_SEPERATE) + MAX_BODY_SIZE_DIGITS_NUM]; 
	index += sprintf((contLengthHeader + index),HEADER_CONTENT_LENGTH HEADER_SEPERATE);
	index += sprintf((contLengthHeader + index),"%d",strlen(bodyUserCode));

	const char* headers[] ={
		HEADER_HOST HEADER_SEPERATE SHORT_URL,
		contLengthHeader,
		HEADER_CONTENT_TYPE HEADER_SEPERATE TYPE_APP_JSON
	};

	uint16_t headersSize = sizeof(headers)/sizeof(headers[0]);

	httpMsgString = BuildHttpPostMsg(URL_EXT_REG_GATEWAY, headers, headersSize, bodyUserCode);
	cmndsParams.atQhttppost.postBodySize = strlen(httpMsgString);
	cmndsParams.atQhttppost.inputTimeout = HTTP_SEND_RECEIVE_TIMEOUT;
	cmndsParams.atQhttppost.responseTimeout = HTTP_SEND_RECEIVE_TIMEOUT;

	status = sentAndReadHttpMsg(cmndsParams, AT_CMD_QHTTPPOST, httpMsgString);
	return status;
}

static bool verifyConnectionResponse()
{
	cJSON * checkDataJson = cJSON_GetObjectItem(s_lastJson, "access_token");
	if(checkDataJson)
	{
		memcpy(s_accessToken, cJSON_GetStringValue(checkDataJson), SIZE_OF_ACCESS_TOKEN_FIRST);
		checkDataJson = cJSON_GetObjectItem(s_lastJson, "refresh_token");
		memcpy(s_refreshToken, cJSON_GetStringValue(checkDataJson), SIZE_OF_REFRESH_TOKEN);
		cJSON_Delete(s_lastJson);
		return true;
	}
	else
	{
		checkDataJson = cJSON_GetObjectItem(s_lastJson, "error");
		__ASSERT((checkDataJson),"cJSON_GetObjectItem unexpected fail");
		int validError = strcmp("authorization_pending",cJSON_GetStringValue(checkDataJson));
		__ASSERT((validError == 0),"cJSON_GetObjectItem unexpected fail");
		cJSON_Delete(s_lastJson);
		OsalSleep(TIME_TO_RETRY_TOKEN_RECEIVE);
		return false;
	}
}

static SDK_STAT getConnectionAccessAndRefreshToken(char * bodyDeviceCode)
{
	SDK_STAT status = SDK_SUCCESS;
	AtCmndsParams cmndsParams = {0};
	char * httpMsgString = NULL;
	int index = 0;
	bool receivedResponse = false;

	char contLengthHeader[sizeof(HEADER_CONTENT_LENGTH) + sizeof(HEADER_SEPERATE) + MAX_BODY_SIZE_DIGITS_NUM]; 
	index += sprintf((contLengthHeader + index),HEADER_CONTENT_LENGTH HEADER_SEPERATE);
	index += sprintf((contLengthHeader + index),"%d",strlen(bodyDeviceCode));

	const char* headers[] ={
		HEADER_HOST HEADER_SEPERATE SHORT_URL,
		contLengthHeader,
		HEADER_CONTENT_TYPE HEADER_SEPERATE TYPE_APP_JSON
	};

	uint16_t headersSize = sizeof(headers)/sizeof(headers[0]);

	httpMsgString = BuildHttpPostMsg(URL_EXT_GET_TOKENS, headers, headersSize, bodyDeviceCode);
	cmndsParams.atQhttppost.postBodySize = strlen(httpMsgString);
	cmndsParams.atQhttppost.inputTimeout = HTTP_SEND_RECEIVE_TIMEOUT;
	cmndsParams.atQhttppost.responseTimeout = HTTP_SEND_RECEIVE_TIMEOUT;

	while(!receivedResponse)
	{
		status = sentAndReadHttpMsg(cmndsParams, AT_CMD_QHTTPPOST, httpMsgString);
		RETURN_ON_FAIL(status, SDK_SUCCESS, status);
		receivedResponse = verifyConnectionResponse();
	}

	return SDK_SUCCESS;
}

SDK_STAT NetworkInit()
{
    SDK_STAT status = SDK_SUCCESS;
	char responseBuffer[RESPONSE_BUFFER_SIZE] = {0};
    AtCmndsParams cmndsParams = {0};
	
	k_event_init(&s_responseEvent);

	status = ModemInit(modemReadCallback);
	__ASSERT((status == SDK_SUCCESS),"ModemInit internal fail");
	status = modemResponseWait(MODEM_READY_VERITY_WORD, strlen(MODEM_READY_VERITY_WORD), MODEM_WAKE_TIMEOUT);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	status = AtExecuteCmd(AT_CMD_TEST);
    __ASSERT((status == SDK_SUCCESS),"AtExecuteCmd internal fail");
	status = modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	status = AtExecuteCmd(AT_CMD_QCCID);
    __ASSERT((status == SDK_SUCCESS),"AtExecuteCmd internal fail");
	status = modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	cmndsParams.atCtzrParams.mode = MODE_ENABLE_EXTENDED_TIME_ZONE_CHANGE;
	status = AtWriteCmd(AT_CMD_CTZR, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	status = modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	cmndsParams.atCtzuParams.mode = MODE_ENABLE_AUTOMATIC_TIME_ZONE_UPDATE;
	status = AtWriteCmd(AT_CMD_CTZU, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	status = modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	cmndsParams.atQcfgParams.cmd = EXTENDED_CONFIGURE_RATS_SEARCHING_SEQUENCE;
	cmndsParams.atQcfgParams.mode = SEARCHING_SEQUENCE;
	cmndsParams.atQcfgParams.effect = TAKE_EFFECT_IMMEDIATELY;
	status = AtWriteCmd(AT_CMD_QCFG, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	status = modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	cmndsParams.atQcfgParams.cmd = EXTENDED_CONFIGURE_RATS_TO_BE_SEARCHED;
	cmndsParams.atQcfgParams.mode = SCAN_MODE_GSM_ONLY;
	cmndsParams.atQcfgParams.effect = TAKE_EFFECT_IMMEDIATELY;
	status = AtWriteCmd(AT_CMD_QCFG, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	status = modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	cmndsParams.atQcfgParams.cmd = EXTENDED_CONFIGURE_NETWORK_SEARCH_UNDER_LTE_RAT;
	cmndsParams.atQcfgParams.mode = NETWORK_CATEGORY_EMTC;
	cmndsParams.atQcfgParams.effect = TAKE_EFFECT_IMMEDIATELY;
	status = AtWriteCmd(AT_CMD_QCFG, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	status = modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	cmndsParams.atCgdcontParams.cid = PDP_CONTEXT_IDENTIFIER;
	cmndsParams.atCgdcontParams.type = PDP_TYPE_IPV4V6;
	cmndsParams.atCgdcontParams.apn = ACCESS_POINT_NAME;
	status = AtWriteCmd(AT_CMD_CGDCONT, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	status = modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	cmndsParams.atCereg.status = NETWORK_REGISTRATION_ENABLE;
	status = AtWriteCmd(AT_CMD_CEREG, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	status = modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	while(true)
	{
		status = AtReadCmd(AT_CMD_CEREG);
		__ASSERT((status == SDK_SUCCESS),"AtReadCmd internal fail");
		status = atEnumToResponseString(AT_CMD_CEREG, responseBuffer, RESPONSE_BUFFER_SIZE);
		__ASSERT((status == SDK_SUCCESS),"atEnumToResponseString internal fail");
		status = modemResponseWait(responseBuffer, strlen(responseBuffer), MODEM_WAKE_TIMEOUT);
		RETURN_ON_FAIL(status, SDK_SUCCESS, status);

		if(strcmp(s_responseExtraPayload, MODEM_CEREG_SUCCESS_PAYLOAD) == 0)
		{
			break;
		}

		OsalSleep(TIME_TO_RETRY_CEREG_PAYLOAD);
	}

	status = AtReadCmd(AT_CMD_COPS);
    __ASSERT((status == SDK_SUCCESS),"AtReadCmd internal fail");
	status = modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	status = AtExecuteCmd(AT_CMD_QCSQ);
    __ASSERT((status == SDK_SUCCESS),"AtExecuteCmd internal fail");
	status = modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	cmndsParams.atQlts.mode = QUERY_THE_CURRENT_LOCAL_TIME;
	status = AtWriteCmd(AT_CMD_QLTS, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	status = modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

    if (SDK_SUCCESS != UpdateConfGatewayId())
    {
        printk("Failed setting imei-based id");
    }

	return SDK_SUCCESS;
}

bool IsNetworkAvailable()
{
	SDK_STAT status = SDK_SUCCESS;
	bool isNetworkAvailable = false;
	char responseBuffer[RESPONSE_BUFFER_SIZE] = {0};
	bool currentLTEConfig = s_isLTEConfigurationDone;

	s_isLTEConfigurationDone = false;

	status = AtReadCmd(AT_CMD_CEREG);
	__ASSERT((status == SDK_SUCCESS),"AtReadCmd internal fail");
	status = atEnumToResponseString(AT_CMD_CEREG, responseBuffer, RESPONSE_BUFFER_SIZE);
	__ASSERT((status == SDK_SUCCESS),"atEnumToResponseString internal fail");
	status = modemResponseWait(responseBuffer, strlen(responseBuffer), MODEM_WAKE_TIMEOUT);
	RETURN_ON_FAIL(status, SDK_SUCCESS, false);

	isNetworkAvailable = (strcmp(s_responseExtraPayload, MODEM_CEREG_SUCCESS_PAYLOAD) == 0) ? true : false;
	s_isLTEConfigurationDone = currentLTEConfig;
	
	return isNetworkAvailable;
}

SDK_STAT ConnectToNetwork()
{
	SDK_STAT status = SDK_SUCCESS;
	char bodyDeviceCode[SIZE_OF_ADDRESS_BUFFER + sizeof(BODY_REG_POST) + SIZE_OF_DEVICE_CODE + 1] = {0};
	char bodyUserCode[SIZE_OF_ADDRESS_BUFFER + sizeof(BODY_REG_POST)  + SIZE_OF_USER_CODE + 1] = {0};
	char * authorizationHeader = NULL;
	int authIndex = 0;
	int deviceIndex = 0;
	int userIndex = 0;

	authorizationHeader = (char*)OsalCalloc(sizeof(HEADER_AUTHORIZATION) + sizeof(HEADER_SEPERATE) + sizeof(CONNECTION_TOKEN_TYPE) + SIZE_OF_ACCESS_CONNECTION_TOKEN + 1);
	if(!authorizationHeader)
	{
        printk("Allocation failed!\n");
		return SDK_FAILURE;
	}

	status = setConnectionUrl();
    printk("setconnectionUrl %d\n", status);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	authIndex += sprintf((authorizationHeader + authIndex),HEADER_AUTHORIZATION HEADER_SEPERATE CONNECTION_TOKEN_TYPE);
	authIndex += sprintf((authorizationHeader + authIndex)," ");

	status = getConnectionAccessToken(authorizationHeader + authIndex);
    printk("getconnectionaccesstoken %d\n", status);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);
	status = setConnectionGateway(authorizationHeader);
    printk("setconnectiongateway %d\n", status);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	userIndex += sprintf((bodyUserCode + userIndex),"%s",BODY_REG_USR_CODE_PRE(s_addressStringBuffer));
	deviceIndex += sprintf((bodyDeviceCode + deviceIndex),"%s",BODY_REG_DEV_CODE_PRE(s_addressStringBuffer));

	getConnectionCodes((bodyUserCode + userIndex), (bodyDeviceCode + deviceIndex));

	userIndex += SIZE_OF_USER_CODE;
	deviceIndex += SIZE_OF_DEVICE_CODE;

	userIndex += sprintf((bodyUserCode + userIndex),BODY_REG_POST);
	deviceIndex += sprintf((bodyDeviceCode + deviceIndex),BODY_REG_POST);

	status = registerConnectionGateway(bodyUserCode);
    if (status != SDK_SUCCESS)
    {
        OsalFree(authorizationHeader);
        return status;
    }
	status = getConnectionAccessAndRefreshToken(bodyDeviceCode); //refresh token is in s_refreshtoken after this

	OsalFree(authorizationHeader);

	return status;
}

SDK_STAT UpdateAccessToken(Token refreshToken, Token * accessToken, uint32_t * accessTokenExpiry)
{
	SDK_STAT status = SDK_SUCCESS;
    AtCmndsParams cmndsParams = {0};
	char * httpMsgString = NULL;
	char extendedUrlWithRefreshToken[sizeof(URL_EXT_UPDATE_ACCESS) + SIZE_OF_REFRESH_TOKEN + 1] = {0};
	bool currentLTEConfig = s_isLTEConfigurationDone;

	s_isLTEConfigurationDone = false;

	if(!refreshToken || !accessToken || !accessTokenExpiry)
	{
		return SDK_INVALID_PARAMS;
	}

	status = setConnectionUrl();
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	sprintf(extendedUrlWithRefreshToken, URL_EXT_UPDATE_ACCESS);
	sprintf((extendedUrlWithRefreshToken + sizeof(URL_EXT_UPDATE_ACCESS) - 1),"%s", (char*)refreshToken);

	const char* headers[] ={
		HEADER_HOST HEADER_SEPERATE SHORT_URL,
	};

	uint16_t headersSize = sizeof(headers)/sizeof(headers[0]);

	httpMsgString = BuildHttpPostMsg(extendedUrlWithRefreshToken, headers, headersSize, NULL);
	cmndsParams.atQhttppost.postBodySize = strlen(httpMsgString);
	cmndsParams.atQhttppost.inputTimeout = HTTP_SEND_RECEIVE_TIMEOUT;
	cmndsParams.atQhttppost.responseTimeout = HTTP_SEND_RECEIVE_TIMEOUT;

	status = sentAndReadHttpMsg(cmndsParams, AT_CMD_QHTTPPOST, httpMsgString);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	cJSON * checkDataJson = cJSON_GetObjectItem(s_lastJson, "access_token");

	if(checkDataJson)
	{
		memcpy(s_accessToken, cJSON_GetStringValue(checkDataJson), SIZE_OF_ACCESS_TOKEN_FULL);
		*accessToken = s_accessToken;

		checkDataJson = cJSON_GetObjectItem(s_lastJson, "expires_in");
		if(checkDataJson)
		{
			*accessTokenExpiry = (uint32_t)cJSON_GetNumberValue(checkDataJson);
		}
		else
		{
			s_isLTEConfigurationDone = currentLTEConfig;
			cJSON_Delete(s_lastJson);
			return SDK_FAILURE;
		}

		s_isLTEConfigurationDone = currentLTEConfig;
		cJSON_Delete(s_lastJson);
		return SDK_SUCCESS;
	}
	else
	{
		s_isLTEConfigurationDone = currentLTEConfig;
		cJSON_Delete(s_lastJson);
		return SDK_NOT_FOUND;
	}
}

static SDK_STAT openSSLLink()
{
	SDK_STAT status = SDK_SUCCESS;
	const char * mqttServerPtr = NULL;
	char responseBuffer[RESPONSE_BUFFER_SIZE] = {0};
    AtCmndsParams cmndsParams = {0};

	status = GetMqttServer(&mqttServerPtr);
	assert(status == SDK_SUCCESS);

	cmndsParams.atQsslopen.pdpctxId = MQTT_PDP_CTX_ID;
	cmndsParams.atQsslopen.sslctxId = MQTT_SSL_CTX_ID;
	cmndsParams.atQsslopen.clientId = MQTT_CLIENT_IDX;
	cmndsParams.atQsslopen.serverAddr = mqttServerPtr;
	cmndsParams.atQsslopen.serverPort = MQTT_SERV_PORT;
	cmndsParams.atQsslopen.accessMode = MQTT_ACCESS_BUFFER_ACCESS;

	status = AtWriteCmd(AT_CMD_QSSLOPEN, &cmndsParams);
    printk("qsslopen in openssllink %d\n", status);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	status = atEnumToResponseString(AT_CMD_QSSLOPEN, responseBuffer, RESPONSE_BUFFER_SIZE);
    printk("atenumtoresponsestring in openssllink %d\n", status);
	__ASSERT((status == SDK_SUCCESS),"atEnumToResponseString internal fail");
	status = modemResponseWait(responseBuffer, strlen(responseBuffer), MODEM_WAKE_TIMEOUT);	
    printk("responsebuffer in openssllink %d\n", status);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	return SDK_SUCCESS;
}

static SDK_STAT sendSSLPacket(int clientId, int packetLength)
{
	SDK_STAT status = SDK_SUCCESS;
    AtCmndsParams cmndsParams = {0};

	if(packetLength == 0)
	{
		return SDK_INVALID_PARAMS;
	}

	cmndsParams.atQsslsend.clientId = clientId;
	cmndsParams.atQsslsend.payloadLength = packetLength;

	status = AtWriteCmd(AT_CMD_QSSLSEND, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	status = modemResponseWait(MQTT_INPUT_STRING, strlen(MQTT_INPUT_STRING), MODEM_WAKE_TIMEOUT);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);
	ModemSend(s_mqttPacketBuff,packetLength);
	status = modemResponseWait(MQTT_BROKER_RESPONSE_STRING, strlen(MQTT_BROKER_RESPONSE_STRING), MODEM_WAKE_TIMEOUT);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	return SDK_SUCCESS;
}

static SDK_STAT connectToSSL()
{
	int len = 0;
	SDK_STAT status = SDK_SUCCESS;
	const char * gateWayNamePtr = NULL;
	const char * accountIdPtr = NULL;
	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
	uint8_t errCode[] = MQTT_ERROR_CODE_CONNECT;

	status = GetGatewayId(&gateWayNamePtr);
	assert(status == SDK_SUCCESS);

	status = GetAccountID(&accountIdPtr);
	assert(status == SDK_SUCCESS);

	data.clientID.cstring = (char *)gateWayNamePtr;
	data.keepAliveInterval = MQTT_KEEP_ALIVE_INTERVAL;
	data.cleansession = MQTT_CLEAN_SESSION;
	data.username.cstring = (char *)accountIdPtr;
	data.password.cstring = (char *)s_accessToken; // here acceess token

	len = MQTTSerialize_connect(s_mqttPacketBuff, sizeof(s_mqttPacketBuff), &data);
	__ASSERT(len,"MQTTSerialize_connect internal fail");

	status = sendSSLPacket(MQTT_CLIENT_IDX, len);
	__ASSERT((status == SDK_SUCCESS),"sendSslPacket internal fail");

	status = modemResponseWait((char*)errCode, sizeof(errCode), MODEM_WAKE_TIMEOUT);
	return status;
}

SDK_STAT SubscribeToTopic(char * topic)
{
	int req_qos = 0;
	int len = 0;
	int numOfTopicFiltersAndQosArray = 0;
	static int packetId = 0;
	SDK_STAT status = SDK_SUCCESS;
	MQTTString topicString = MQTTString_initializer;
	uint8_t errCode[] = MQTT_ERROR_CODE_SUBSCRIBE;

	if(!topic) 
	{
		return SDK_INVALID_PARAMS;
	}

	topicString.cstring = topic;
	numOfTopicFiltersAndQosArray++;

	len = MQTTSerialize_subscribe(s_mqttPacketBuff, sizeof(s_mqttPacketBuff), MQTT_PACKET_DEFAULT_DUP, 
									(++packetId), numOfTopicFiltersAndQosArray, &topicString, &req_qos);
	__ASSERT(len,"MQTTSerialize_subscribe internal fail");

	status = sendSSLPacket(MQTT_CLIENT_IDX, len);
	if(status != SDK_SUCCESS)
	{
        printk("sendSSLPacket %d", status);
		return status;
	}

	status = modemResponseWait((char*)errCode, sizeof(errCode), MODEM_WAKE_TIMEOUT);
	if(status != SDK_SUCCESS)
	{
        printk("modemresponsewait errCode %d", status);
		return status;
	}

	return SDK_SUCCESS;
}

conn_handle ConnectToServer()
{
	SDK_STAT status = SDK_SUCCESS;
    AtCmndsParams cmndsParams = {0};

	s_connHandle = (conn_handle)CONNECTION_HANDLE;

	status = openSSLLink();
    printk("openssllink %d\n", status);
	RETURN_ON_FAIL(status, SDK_SUCCESS, NULL);

	cmndsParams.atQiswtmd.clientId = MQTT_CLIENT_IDX;
	cmndsParams.atQiswtmd.accessMode = MQTT_ACCESS_DIRECT_PUSH;

	status = AtWriteCmd(AT_CMD_QISWTMD, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	status = modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
	__ASSERT((status == SDK_SUCCESS),"modemResponseWait internal fail");

	connectToSSL();

	ModemSend(TURN_OFF_AT_CMD_ECHO,strlen(TURN_OFF_AT_CMD_ECHO));
	status = modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
	__ASSERT((status == SDK_SUCCESS),"modemResponseWait internal fail");

	s_isLTEConfigurationDone = true;

	return s_connHandle;
}

SDK_STAT NetSendMQTTPacket(const char* topic, void* pkt, uint32_t length)
{
	int len = 0;
	static unsigned short packetId = 0;

	SDK_STAT status = SDK_SUCCESS;
    AtCmndsParams cmndsParams = {0};
	MQTTString topicString = MQTTString_initializer;

	if(!pkt || !topic || length == 0)
	{
		return SDK_INVALID_PARAMS;
	}
	//casting to work with the library. topic is still const
	topicString.cstring = (char*)topic;
	len = MQTTSerialize_publish(s_mqttPacketBuff, sizeof(s_mqttPacketBuff), MQTT_PACKET_DEFAULT_DUP, 
								MQTT_PACKET_DEFAULT_QOS, MQTT_PACKET_DEFAULT_FLAG, (++packetId), 
								topicString, (unsigned char*)pkt, length);

	cmndsParams.atQsslsend.clientId = MQTT_CLIENT_IDX;
	cmndsParams.atQsslsend.payloadLength = len;

	status = AtWriteCmd(AT_CMD_QSSLSEND, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	status = modemResponseWait(MQTT_INPUT_STRING, strlen(MQTT_INPUT_STRING), MQTT_PUBLISH_TIME_RESPONSE);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	ModemSend(s_mqttPacketBuff,len);
	status = modemResponseWait(MQTT_GOOD_PUBLISH_RESPOSE, strlen(MQTT_GOOD_PUBLISH_RESPOSE), MQTT_PUBLISH_TIME_RESPONSE);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	return SDK_SUCCESS;
}

SDK_STAT RegisterNetReceiveMQTTPacketCallback(NetMQTTPacketCB cb)
{
	if(!cb)
    {
        return SDK_INVALID_PARAMS;
    }

    s_receivedMQTTPacketHandle = cb;

    return SDK_SUCCESS;
}

static SDK_STAT readRefreshTokenFromFlashToStatic(char * compareBuff, size_t sizeOfBuff)
{
	SDK_STAT status = SDK_SUCCESS;

	if(!compareBuff || (sizeOfBuff < sizeof(s_refreshToken)))
	{
		return SDK_INVALID_PARAMS;
	}

	uint8_t * pageFromFlash = OsalMalloc(SIZE_OF_FLASH_PAGE);
	if(!pageFromFlash)
	{
		return SDK_FAILURE;
	}

	status = FlashRead(FLASH_PAGE_TOKENS, pageFromFlash);
	if(status != SDK_SUCCESS)
	{
		OsalFree(pageFromFlash);
		return status;
	}

	if(IS_NO_REFRESH_TOKEN(pageFromFlash,compareBuff))
	{
		OsalFree(pageFromFlash);
		return SDK_NOT_FOUND;
	}

	memcpy(s_refreshToken, pageFromFlash, sizeOfBuff);
	OsalFree(pageFromFlash);

	return SDK_SUCCESS;
}

SDK_STAT GetRefreshToken(Token* refreshToken)
{
	SDK_STAT status = SDK_SUCCESS;
	char compareBuff[sizeof(s_refreshToken)] = {0};

	if(!refreshToken)
	{
        printk("invalid refreshtoken parameter\n");
		return SDK_INVALID_PARAMS;
	}

	if(IS_NO_REFRESH_TOKEN(s_refreshToken, compareBuff))
	{
		memset(compareBuff, CHAR_MAX, sizeof(compareBuff));
		status = readRefreshTokenFromFlashToStatic(compareBuff, sizeof(compareBuff));
		if(status != SDK_SUCCESS)
		{
            printk("readrefreshtokenfromflashtostatic %d\n", status);
			return status;
		}
	}

	*refreshToken = (Token)s_refreshToken;
	return SDK_SUCCESS;
}

SDK_STAT GetAccessToken(Token* accessToken)
{
	char compareBuff[sizeof(s_accessToken)] = {0};

	if(!accessToken)
	{
		return SDK_INVALID_PARAMS;
	}

	if(memcmp(s_accessToken,compareBuff,sizeof(s_accessToken)) == 0)
	{
		return SDK_NOT_FOUND;
	}

	*accessToken = s_accessToken;
	return SDK_SUCCESS;
}

SDK_STAT UpdateRefreshToken(Token refreshToken)
{
	SDK_STAT status = SDK_SUCCESS;
	uint32_t sizeOfRefreshToken = 0;
	uint32_t allignedSized = 0;
	char * zeroPaddedRefreshToken = NULL;

	if(!refreshToken)
	{
		return SDK_INVALID_PARAMS;
	}

	sizeOfRefreshToken = strlen((char*)refreshToken);
	allignedSized = sizeOfRefreshToken + (SIZE_OF_WORD - (sizeOfRefreshToken % SIZE_OF_WORD));
	zeroPaddedRefreshToken = OsalCalloc(allignedSized);
	if(!zeroPaddedRefreshToken)
	{
		return SDK_FAILURE;
	}

	status = FlashErase(FLASH_PAGE_TOKENS);
	if(status != SDK_SUCCESS)
	{
		return SDK_FAILURE;
	}

	memcpy(zeroPaddedRefreshToken,(char*)refreshToken,sizeOfRefreshToken);

	status = FlashWrite(FLASH_PAGE_TOKENS, refreshToken, allignedSized);
	if(status != SDK_SUCCESS)
	{
		return SDK_FAILURE;
	}

	OsalFree(zeroPaddedRefreshToken);
	return SDK_SUCCESS;
}

SDK_STAT ReconnectToNetwork()
{
    SDK_STAT status = SDK_SUCCESS;
	char responseBuffer[RESPONSE_BUFFER_SIZE] = {0};
    AtCmndsParams cmndsParams = {0};

	cmndsParams.atCereg.status = NETWORK_REGISTRATION_ENABLE;
	status = AtWriteCmd(AT_CMD_CEREG, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	status = modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	while(true)
	{
		status = AtReadCmd(AT_CMD_CEREG);
		__ASSERT((status == SDK_SUCCESS),"AtReadCmd internal fail");
		status = atEnumToResponseString(AT_CMD_CEREG, responseBuffer, RESPONSE_BUFFER_SIZE);
		__ASSERT((status == SDK_SUCCESS),"atEnumToResponseString internal fail");
		status = modemResponseWait(responseBuffer, strlen(responseBuffer), MODEM_WAKE_TIMEOUT);
		RETURN_ON_FAIL(status, SDK_SUCCESS, status);

		if(strcmp(s_responseExtraPayload, MODEM_CEREG_SUCCESS_PAYLOAD) == 0)
		{
			break;
		}

		OsalSleep(TIME_TO_RETRY_CEREG_PAYLOAD);
	}

	status = AtReadCmd(AT_CMD_COPS);
    __ASSERT((status == SDK_SUCCESS),"AtReadCmd internal fail");
	status = modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	status = AtExecuteCmd(AT_CMD_QCSQ);
    __ASSERT((status == SDK_SUCCESS),"AtExecuteCmd internal fail");
	status = modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	cmndsParams.atQlts.mode = QUERY_THE_CURRENT_LOCAL_TIME;
	status = AtWriteCmd(AT_CMD_QLTS, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	status = modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD), MODEM_WAKE_TIMEOUT);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

    return SDK_SUCCESS;
}

SDK_STAT RegisterConnStateChangeCB(handleConnStateChangeCB cb, conn_handle handle)
{
    if(!cb || (handle != s_connHandle))
    {
        return SDK_INVALID_PARAMS;
    }

    s_receivedConnStateHandle = cb;

    return SDK_SUCCESS;
}

char *GetIMEI()
{
	AtCmndsParams cmndsParams = {0};
    cmndsParams.atCgsn.getImei = 1;
    SDK_STAT status;

    s_anyResponse = true;
    status = AtExecuteCmd(AT_CMD_CGSN);
	__ASSERT((status == SDK_SUCCESS),"AtExecuteCmd internal fail");
	modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD) + 1, MODEM_WAKE_TIMEOUT);
    OsalSleep(200);

    if (status != SDK_SUCCESS || s_anyResponse == true)
    {
        return NULL;
    }

    return s_responseExtraPayload;
}
