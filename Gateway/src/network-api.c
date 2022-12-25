#include "network-api.h"
#include "at-commands.h"
#include "modem-drv.h"
#include "cJSON.h"
#include "httpStringBuilder.h"
#include "williotSdkJson.h"
#include "osal.h"

#include <zephyr/zephyr.h>
#include <assert.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>

#include <string.h>

#define SIZE_OF_COMPARE_STRING				(128)
#define MODEM_WAKE_TIMEOUT					(60000)
#define AT_CMD_TIMEOUT						(1000)
#define RESPONSE_EVENT_BIT_SET				(0x001)

#define IS_JSON_PREFIX(buff)			  	(*(buff) == '{')

#define SEARCHING_SEQUENCE					"020202"
#define TAKE_EFFECT_IMMEDIATELY				(1)
#define SCAN_MODE_GSM_ONLY					"0"
#define NETWORK_CATEGORY_EMTC				"0"
#define PDP_CONTEXT_IDENTIFIER				(1)
#define ACCESS_POINT_NAME					"uinternet"

#define NETWORK_AVAILABLE_CONTEX_ID 		(1)
#define ADDRESS_TO_PING 					"www.google.com"
#define MODEM_RESPONSE_VERIFY_WORD			"OK"
#define MODEM_HTTP_URL_VERIFY_WORD			"CONNECT"
#define MODEM_READY_VERITY_WORD				"APP RDY"
#define MODEM_CEREG_SUCCESS_PAYLOAD			" 1,1"
#define GATEWAY_NAME						"tandemGW"
#define TIME_TO_RETRY_CEREG_PAYLOAD			(10000)

#define RESPONSE_BUFFER_SIZE				(50)
#define JWT_TOKEN							"Yjk2MjAyY2ItMDcyZi00NzRiLThmNjMtOTMyOTA5N2IxOTg1OnhJNUhHSC1KcXZxNGN6NzVsZFYwUDY4Ul8xXzg3Q1lFRGl3RVJMcFJ6dlU"
#define OWNER_ID							"959266658936"
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
#define TIME_TO_RETRY_TOKEN_RECEIVE			(30000)
#define SIZE_OF_RESPONSE_PAYLOAD			(20)

#define HEADER_SEPERATE					    ": "
#define HEADER_HOST							"Host"
#define HEADER_AUTHORIZATION				"Authorization"
#define HEADER_CONTENT_LENGTH				"Content-Length"
#define HEADER_CONTENT_TYPE					"Content-Type"

#define SHORT_URL							"api.wiliot.com"
#define CONNECTION_URL						"https://"SHORT_URL
#define TYPE_APP_JSON						"application/json"
#define URL_EXT_ACCESS_TOKEN				"/v1/auth/token/api"
#define URL_EXT_GATEWAY_OWN					"/v1/owner/"OWNER_ID"/gateway"
#define URL_EXT_DEVICE_AUTH					"/v1/gateway/device-authorize"
#define URL_EXT_REG_GATEWAY					"/v2/gateway/registry"
#define URL_EXT_GET_TOKENS					"/v2/gateway/token"
#define URL_EXT_UPDATE_ACCESS				"/v1/gateway/refresh?refresh_token="

#define MAX_BODY_SIZE_DIGITS_NUM			(5)
#define BODY_GATEWAY						"{\r\n    \"gateways\":[\""GATEWAY_NAME"\"]\r\n}"
#define BODY_REG_USR_CODE_PRE				"{\r\n    \"gatewayId\":\""GATEWAY_NAME"\",\r\n    \"userCode\": \""
#define BODY_REG_DEV_CODE_PRE				"{\r\n    \"gatewayId\":\""GATEWAY_NAME"\",\r\n    \"deviceCode\": \""
#define BODY_REG_POST						"\"\r\n}"

typedef struct{
	bool isWaitingResponse;
	char * cmprStr;
	size_t strSize;
} CallbackStruct;

static struct k_event s_responseEvent;
static CallbackStruct s_callbackStruct;
static cJSON *s_lastJson = NULL;
static char s_accessToken[SIZE_OF_ACCESS_TOKEN_FULL + 1] = {0};
static char s_refreshToken[SIZE_OF_REFRESH_TOKEN + 1] = {0};
static char s_responseExtraPayload[SIZE_OF_RESPONSE_PAYLOAD + 1] = {0};

static void modemReadCallback(uint8_t* buff, uint16_t buffSize)
{	
	if(IS_JSON_PREFIX(buff))
	{
		char * cJsonString = NULL;
		// Don't forget to free 
		s_lastJson = cJSON_Parse(buff);

		cJsonString = cJSON_Print(s_lastJson);
		printk("%s\r\n",cJsonString);
		FreeJsonString(cJsonString);
	}
	else
	{
		printk("%s\r\n",buff);
	}

	if(strncmp(buff, s_callbackStruct.cmprStr, s_callbackStruct.strSize) == 0)
	{
		if(buffSize > s_callbackStruct.strSize)
		{	
			__ASSERT(((buffSize - s_callbackStruct.strSize) < SIZE_OF_RESPONSE_PAYLOAD),"SIZE_OF_RESPONSE_PAYLOAD is too small");
			sprintf(s_responseExtraPayload, "%s", (buff + s_callbackStruct.strSize));
		}

		k_event_post(&s_responseEvent, RESPONSE_EVENT_BIT_SET);
	}

}

static inline void modemResponseWait(char * waitString, size_t stringSize)
{
	int waitEventState = 0;

	s_callbackStruct.cmprStr = waitString;
	s_callbackStruct.strSize = stringSize;
	waitEventState = k_event_wait(&s_responseEvent, RESPONSE_EVENT_BIT_SET, true, K_MSEC(MODEM_WAKE_TIMEOUT));
	__ASSERT((waitEventState != 0),"k_event_wait timeout");
}

static void setConnectionUrl(void)
{
	SDK_STAT status = SDK_SUCCESS;
	AtCmndsParams cmndsParams = {0};

	cmndsParams.atQhttpcfg.cmd = HTTP_CONFIGURATION_PDP_CONTEXT_ID;
	cmndsParams.atQhttpcfg.value = HTTP_CONFIG_CONTEX_ID;
	status = AtWriteCmd(AT_CMD_QHTTPCFG, &cmndsParams);
	__ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD));

	cmndsParams.atQhttpcfg.cmd = HTTP_CONFIGURATION_REQUEST_HEADER;
	cmndsParams.atQhttpcfg.value = HTTP_CONFIG_REQUEST_HEADER;
	status = AtWriteCmd(AT_CMD_QHTTPCFG, &cmndsParams);
	__ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD));

	cmndsParams.atQhttpurl.urlLength = strlen(CONNECTION_URL);
	cmndsParams.atQhttpurl.urlTimeout = HTTP_SEND_RECEIVE_TIMEOUT;
	status = AtWriteCmd(AT_CMD_QHTTPURL, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	modemResponseWait(MODEM_HTTP_URL_VERIFY_WORD, strlen(MODEM_HTTP_URL_VERIFY_WORD));
	ModemSend(CONNECTION_URL, cmndsParams.atQhttpurl.urlLength);
	modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD));
}

static void sentAndReadHttpMsg(AtCmndsParams cmndsParams, eAtCmds atCmd, char* httpMsgString)
{
	SDK_STAT status = SDK_SUCCESS;
	char responseBuffer[RESPONSE_BUFFER_SIZE] = {0};

	__ASSERT(atCmd == AT_CMD_QHTTPPOST || atCmd == AT_CMD_QHTTPPUT,"sentAndReadHttpMsg not http cmd");
	status = AtWriteCmd(atCmd, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	modemResponseWait(MODEM_HTTP_URL_VERIFY_WORD, strlen(MODEM_HTTP_URL_VERIFY_WORD));
	ModemSend(httpMsgString, strlen(httpMsgString));
	modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD));
	status = atEnumToResponseString(atCmd, responseBuffer, RESPONSE_BUFFER_SIZE);
	__ASSERT((status == SDK_SUCCESS),"atEnumToResponseString internal fail");
	modemResponseWait(responseBuffer, strlen(responseBuffer));

	status = AtWriteCmd(AT_CMD_QHTTPREAD, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	modemResponseWait(MODEM_HTTP_URL_VERIFY_WORD, strlen(MODEM_HTTP_URL_VERIFY_WORD));
	modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD));
	status = atEnumToResponseString(AT_CMD_QHTTPREAD, responseBuffer, RESPONSE_BUFFER_SIZE);
	__ASSERT((status == SDK_SUCCESS),"atEnumToResponseString internal fail");
	modemResponseWait(responseBuffer, strlen(responseBuffer));

}

static void getConnectionAccessToken(char * accessConnectionToken)
{
	AtCmndsParams cmndsParams = {0};
	char * httpMsgString = NULL;

	const char* headers[] ={
		HEADER_HOST HEADER_SEPERATE SHORT_URL,
		HEADER_AUTHORIZATION HEADER_SEPERATE JWT_TOKEN
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
}

static void setConnectionGateway(char * authorizationHeader)
{
	AtCmndsParams cmndsParams = {0};
	char * httpMsgString = NULL;
	int index = 0;

	char contLengthHeader[sizeof(HEADER_CONTENT_LENGTH) + sizeof(HEADER_SEPERATE) + MAX_BODY_SIZE_DIGITS_NUM]; 
	index += sprintf((contLengthHeader + index),HEADER_CONTENT_LENGTH HEADER_SEPERATE);
	index += sprintf((contLengthHeader + index),"%d",strlen(BODY_GATEWAY));

	const char* headers[] ={
		HEADER_HOST HEADER_SEPERATE SHORT_URL,
		contLengthHeader,
		authorizationHeader,
		HEADER_CONTENT_TYPE HEADER_SEPERATE TYPE_APP_JSON
	};

	uint16_t headersSize = sizeof(headers)/sizeof(headers[0]);

	httpMsgString = BuildHttpPutMsg(URL_EXT_GATEWAY_OWN, headers, headersSize, BODY_GATEWAY);
	cmndsParams.atQhttppost.postBodySize = strlen(httpMsgString);
	cmndsParams.atQhttppost.inputTimeout = HTTP_SEND_RECEIVE_TIMEOUT;
	cmndsParams.atQhttppost.responseTimeout = HTTP_SEND_RECEIVE_TIMEOUT;

	sentAndReadHttpMsg(cmndsParams, AT_CMD_QHTTPPUT, httpMsgString);

	cJSON *checkDataJson = cJSON_GetObjectItem(s_lastJson, "data");
	printk("Json response: %s\r\n",cJSON_GetStringValue(checkDataJson));
	cJSON_Delete(s_lastJson);
}

static void getConnectionCodes(char * userCode, char * deviceCode)
{
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

	sentAndReadHttpMsg(cmndsParams, AT_CMD_QHTTPPOST, httpMsgString);

	cJSON * checkDataJson = cJSON_GetObjectItem(s_lastJson, "device_code");
	memcpy(deviceCode, cJSON_GetStringValue(checkDataJson), SIZE_OF_DEVICE_CODE);
	checkDataJson = cJSON_GetObjectItem(s_lastJson, "user_code");
	memcpy(userCode, cJSON_GetStringValue(checkDataJson), SIZE_OF_USER_CODE);
	cJSON_Delete(s_lastJson);
}

static void registerConnectionGateway(char * bodyUserCode)
{
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

	sentAndReadHttpMsg(cmndsParams, AT_CMD_QHTTPPOST, httpMsgString);
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

static void getConnectionAccessAndRefreshToken(char * bodyDeviceCode)
{
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
		sentAndReadHttpMsg(cmndsParams, AT_CMD_QHTTPPOST, httpMsgString);
		receivedResponse = verifyConnectionResponse();
	}
}

SDK_STAT NetworkInit()
{
    SDK_STAT status = SDK_SUCCESS;
	char responseBuffer[RESPONSE_BUFFER_SIZE] = {0};
    AtCmndsParams cmndsParams = {0};

	k_event_init(&s_responseEvent);
    
    status = ModemInit(modemReadCallback);
    __ASSERT((status == SDK_SUCCESS),"ModemInit internal fail");
	modemResponseWait(MODEM_READY_VERITY_WORD, strlen(MODEM_READY_VERITY_WORD));

	status = AtExecuteCmd(AT_CMD_TEST);
    __ASSERT((status == SDK_SUCCESS),"AtExecuteCmd internal fail");
	modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD));

	status = AtExecuteCmd(AT_CMD_QCCID);
    __ASSERT((status == SDK_SUCCESS),"AtExecuteCmd internal fail");
	modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD));

	cmndsParams.atCtzrParams.mode = MODE_ENABLE_EXTENDED_TIME_ZONE_CHANGE;
	status = AtWriteCmd(AT_CMD_CTZR, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD));

	cmndsParams.atCtzuParams.mode = MODE_ENABLE_AUTOMATIC_TIME_ZONE_UPDATE;
	status = AtWriteCmd(AT_CMD_CTZU, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD));

	cmndsParams.atQcfgParams.cmd = EXTENDED_CONFIGURE_RATS_SEARCHING_SEQUENCE;
	cmndsParams.atQcfgParams.mode = SEARCHING_SEQUENCE;
	cmndsParams.atQcfgParams.effect = TAKE_EFFECT_IMMEDIATELY;
	status = AtWriteCmd(AT_CMD_QCFG, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD));

	cmndsParams.atQcfgParams.cmd = EXTENDED_CONFIGURE_RATS_TO_BE_SEARCHED;
	cmndsParams.atQcfgParams.mode = SCAN_MODE_GSM_ONLY;
	cmndsParams.atQcfgParams.effect = TAKE_EFFECT_IMMEDIATELY;
	status = AtWriteCmd(AT_CMD_QCFG, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD));

	cmndsParams.atQcfgParams.cmd = EXTENDED_CONFIGURE_NETWORK_SEARCH_UNDER_LTE_RAT;
	cmndsParams.atQcfgParams.mode = NETWORK_CATEGORY_EMTC;
	cmndsParams.atQcfgParams.effect = TAKE_EFFECT_IMMEDIATELY;
	status = AtWriteCmd(AT_CMD_QCFG, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD));

	cmndsParams.atCgdcontParams.cid = PDP_CONTEXT_IDENTIFIER;
	cmndsParams.atCgdcontParams.type = PDP_TYPE_IPV4V6;
	cmndsParams.atCgdcontParams.apn = ACCESS_POINT_NAME;
	status = AtWriteCmd(AT_CMD_CGDCONT, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD));

	cmndsParams.atCereg.status = NETWORK_REGISTRATION_ENABLE;
	status = AtWriteCmd(AT_CMD_CEREG, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD));

	while(true)
	{
		status = AtReadCmd(AT_CMD_CEREG);
		__ASSERT((status == SDK_SUCCESS),"AtReadCmd internal fail");
		status = atEnumToResponseString(AT_CMD_CEREG, responseBuffer, RESPONSE_BUFFER_SIZE);
		__ASSERT((status == SDK_SUCCESS),"atEnumToResponseString internal fail");
		modemResponseWait(responseBuffer, strlen(responseBuffer));

		if(strcmp(s_responseExtraPayload, MODEM_CEREG_SUCCESS_PAYLOAD) == 0)
		{
			break;
		}

		OsalSleep(TIME_TO_RETRY_CEREG_PAYLOAD);
	}

	status = AtReadCmd(AT_CMD_COPS);
    __ASSERT((status == SDK_SUCCESS),"AtReadCmd internal fail");
	modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD));

	status = AtExecuteCmd(AT_CMD_QCSQ);
    __ASSERT((status == SDK_SUCCESS),"AtExecuteCmd internal fail");
	modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD));

	cmndsParams.atQlts.mode = QUERY_THE_CURRENT_LOCAL_TIME;
	status = AtWriteCmd(AT_CMD_QLTS, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD));

	return SDK_SUCCESS;
}

bool IsNetworkAvailable()
{
	SDK_STAT status = SDK_SUCCESS;
    AtCmndsParams cmndsParams = {0};
	int eventResult = 0;

	cmndsParams.atQping.contextId = NETWORK_AVAILABLE_CONTEX_ID;
	cmndsParams.atQping.host = ADDRESS_TO_PING;
	status = AtWriteCmd(AT_CMD_QPING, &cmndsParams);
    __ASSERT((status == SDK_SUCCESS),"AtWriteCmd internal fail");
	modemResponseWait(MODEM_RESPONSE_VERIFY_WORD, strlen(MODEM_RESPONSE_VERIFY_WORD));
	return (eventResult) ? true : false;
}

SDK_STAT NetSendMQTTPacket(void* pkt, uint32_t length)
{
	return SDK_SUCCESS;
}

SDK_STAT connectToNetwork()
{
	char bodyDeviceCode[sizeof(BODY_REG_DEV_CODE_PRE) + sizeof(BODY_REG_POST) + SIZE_OF_DEVICE_CODE + 1] = {0};
	char bodyUserCode[sizeof(BODY_REG_USR_CODE_PRE) + sizeof(BODY_REG_POST)  + SIZE_OF_USER_CODE + 1] = {0};
	char * authorizationHeader = NULL;
	int authIndex = 0;
	int deviceIndex = 0;
	int userIndex = 0;

	authorizationHeader = (char*)OsalCalloc(sizeof(HEADER_AUTHORIZATION) + sizeof(HEADER_SEPERATE) + sizeof(CONNECTION_TOKEN_TYPE) + SIZE_OF_ACCESS_CONNECTION_TOKEN + 1);

	setConnectionUrl();

	authIndex += sprintf((authorizationHeader + authIndex),HEADER_AUTHORIZATION HEADER_SEPERATE CONNECTION_TOKEN_TYPE);
	authIndex += sprintf((authorizationHeader + authIndex)," ");

	getConnectionAccessToken(authorizationHeader + authIndex);
	setConnectionGateway(authorizationHeader);

	userIndex += sprintf((bodyUserCode + userIndex),BODY_REG_USR_CODE_PRE);
	deviceIndex += sprintf((bodyDeviceCode + deviceIndex),BODY_REG_DEV_CODE_PRE);

	getConnectionCodes((bodyUserCode + userIndex), (bodyDeviceCode + deviceIndex));

	userIndex += SIZE_OF_USER_CODE;
	deviceIndex += SIZE_OF_DEVICE_CODE;

	userIndex += sprintf((bodyUserCode + userIndex),BODY_REG_POST);
	deviceIndex += sprintf((bodyDeviceCode + deviceIndex),BODY_REG_POST);

	registerConnectionGateway(bodyUserCode);
	getConnectionAccessAndRefreshToken(bodyDeviceCode);

	OsalFree(authorizationHeader);

	return SDK_SUCCESS;
}

SDK_STAT UpdateAccessToken(Token accessToken)
{
    AtCmndsParams cmndsParams = {0};
	char * httpMsgString = NULL;
	char extendedUrlWithRefreshToken[sizeof(URL_EXT_UPDATE_ACCESS) + SIZE_OF_REFRESH_TOKEN + 1] = {0};

	sprintf(extendedUrlWithRefreshToken, URL_EXT_UPDATE_ACCESS);
	sprintf((extendedUrlWithRefreshToken + sizeof(URL_EXT_UPDATE_ACCESS) - 1),"%s", s_refreshToken);

	const char* headers[] ={
		HEADER_HOST HEADER_SEPERATE SHORT_URL,
	};

	uint16_t headersSize = sizeof(headers)/sizeof(headers[0]);

	httpMsgString = BuildHttpPostMsg(extendedUrlWithRefreshToken, headers, headersSize, NULL);
	cmndsParams.atQhttppost.postBodySize = strlen(httpMsgString);
	cmndsParams.atQhttppost.inputTimeout = HTTP_SEND_RECEIVE_TIMEOUT;
	cmndsParams.atQhttppost.responseTimeout = HTTP_SEND_RECEIVE_TIMEOUT;

	sentAndReadHttpMsg(cmndsParams, AT_CMD_QHTTPPOST, httpMsgString);

	cJSON * checkDataJson = cJSON_GetObjectItem(s_lastJson, "access_token");
	if(checkDataJson)
	{
		memcpy(s_accessToken, cJSON_GetStringValue(checkDataJson), SIZE_OF_ACCESS_TOKEN_FULL);
		cJSON_Delete(s_lastJson);
		return SDK_SUCCESS;
	}
	else
	{
		return SDK_FAILURE;
	}

}