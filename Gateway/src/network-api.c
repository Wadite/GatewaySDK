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
#include "mqttTopics.h"
#include "downLink.h"
#include "networkManager.h"

#include <assert.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/modem/hl7800.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/mqtt.h>

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
#define API_SEC_KEY						    "Yjk2MjAyY2ItMDcyZi00NzRiLThmNjMtOTMyOTA5N2IxOTg1OnhJNUhHSC1KcXZxNGN6NzVsZFYwUDY4Ul8xXzg3Q1lFRGl3RVJMcFJ6dlU="
/*wiliot prod"MTRlNzAwMjItZTNiNi00ODZjLThkOTQtM2ZmZTk3MTEyM2NjOlFxMXR3THlvRzNSbkRiTm51ZVV3Uk9BeElSUlVxa3pJV0RrLW55NmZLQ3c="*/
/*tandem test:"NTUxNTU1YzAtZTUwNS00MDUwLTlmMzItYTllOTM0MjIyYTQzOm9CUFVITERrSzFZMDNQSEU1Q3Q5QWxJWHk2QzZFMU51aGZBM2M5aGNtazA="*/
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
#define URL_EXT_ACCESS_TOKEN				/*"/test*/"/v1/auth/token/api"
#define URL_EXT_GATEWAY_OWN(buff)			getUrlExtGateWayOwn((buff))
#define URL_EXT_DEVICE_AUTH					/*"/test*/"/v1/gateway/device-authorize"
#define URL_EXT_REG_GATEWAY					/*"/test*/"/v2/gateway/registry"
#define URL_EXT_GET_TOKENS					/*"/test*/"/v2/gateway/token"
#define URL_EXT_UPDATE_ACCESS				/*"/test*/"/v1/gateway/refresh?refresh_token="
#define URL_PRE_REGISTER                    /*"/test*/"/v1/owner/" 

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
#define MQTT_PACKET_DEFAULT_QOS				(MQTT_QOS_0_AT_MOST_ONCE)
#define MQTT_PACKET_DEFAULT_FLAG			(0)
#define MQTT_PUBLISH_TIME_RESPONSE			(1000)
#define MQTT_ERROR_CODE_CONNECT				{32,2,0,0}
#define MQTT_ERROR_CODE_SUBSCRIBE			{144,3,0,1,0}
#define MQTT_CONNECT_TIMEOUT_MS             (60000)
#define MQTT_INPUT_TIMEOUT_MS               (5000)

#define K_LTE_TIMEOUT                       (K_SECONDS(80))
#define HTTPS_PORT                          "443"
#define HTTPS_PORT_INT                      (443)
#define MAX_REQUEST_SIZE                    (2048)
#define MAX_RESPONSE_SIZE                   (4096)
#define CA_CERTIFICATE_TAG                  (42)

#define GET_TOKEN_REQUEST "POST " URL_EXT_ACCESS_TOKEN " HTTP/1.1\r\n"\
        "Host: " SHORT_URL "\r\n"\
        "Connection: close\r\n"\
        "Content-Type: application/json\r\n"\
        "Content-length: 0\r\n"\
        "Authorization: " API_SEC_KEY "\r\n\r\n"

#define SET_GW_REQUEST "PUT %s HTTP/1.1\r\n"\
        "Host: " SHORT_URL "\r\n"\
        "Content-length: %d\r\n"\
        "%s\r\n"\
        "Content-Type: application/json\r\n\r\n"\
        "%s\r\n"

#define GET_CODES_REQUEST "POST " URL_EXT_DEVICE_AUTH " HTTP/1.1\r\n"\
        "Host: " SHORT_URL "\r\n\r\n"\

/* register_connection_request is the same as getconnectionaccessandrefreshtoken request*/
#define REGISTER_CONNECTION_REQUEST "POST " URL_EXT_REG_GATEWAY " HTTP/1.1\r\n"\
        "Host: " SHORT_URL "\r\n"\
        "Content-length: %d\r\n"\
        "Content-Type: application/json\r\n\r\n"\
        "%s\r\n"
#define VALIDATE_REFRESH_REQUEST "POST " URL_EXT_GET_TOKENS " HTTP/1.1\r\n"\
        "Host: " SHORT_URL "\r\n"\
        "Content-length: %d\r\n"\
        "Content-Type: application/json\r\n\r\n"\
        "%s\r\n"

#define UPDATE_TOKEN_REQUEST "POST " URL_EXT_UPDATE_ACCESS "%s HTTP/1.1\r\n"\
        "Host: " SHORT_URL "\r\n\r\n"\

#define AUTH_HEADER_SIZE    (sizeof(HEADER_AUTHORIZATION) + sizeof(HEADER_SEPERATE) + sizeof(CONNECTION_TOKEN_TYPE) + \
                            SIZE_OF_ACCESS_CONNECTION_TOKEN + 1)

#define MAX_URL_EXT_SIZE (48)

static bool verifyConnectionResponse();

typedef enum{
        TLS_PEER_VERIFICATION_NONE     = 0,
        TLS_PEER_VERIFICATION_OPTIONAL = 1,
        TLS_PEER_VERIFICATION_REQUIRED = 2,
}eTlsVerification;

static const char wlt_cert[] = {
        #include "wiliot_till_june_2023.der.inc"
};

typedef struct{
    int bodyLen;
    char *body;
}regAndRefreshParam;

typedef struct{
	bool isWaitingResponse;
	char * cmprWord;
	size_t wordSize;
} CallbackStruct;

static struct k_event s_responseEvent;
static char s_response[MAX_RESPONSE_SIZE];
static CallbackStruct s_callbackStruct;
static cJSON *s_lastJson = NULL;
static char s_accessToken[SIZE_OF_ACCESS_TOKEN_FULL + 1] = {0};
static char s_refreshToken[SIZE_OF_REFRESH_TOKEN + 1] = {0};
static char s_responseExtraPayload[SIZE_OF_RESPONSE_PAYLOAD + 1] = {0};
static bool s_isLTEConfigurationDone = false;
static unsigned char s_mqttPacketBuff[MQTT_MAX_PACKET_SIZE] = {0};
static char s_addressStringBuffer[SIZE_OF_ADDRESS_BUFFER] = {0};
static char s_requestBuffer[MAX_REQUEST_SIZE] = {0};
NetMQTTPacketCB s_receivedMQTTPacketHandle = 0;
handleConnStateChangeCB s_receivedConnStateHandle = 0;
static conn_handle s_connHandle = 0;
static bool s_anyResponse = false;

static bool s_isNetworkReady = false;

static void iface_dns_added_evt_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
                    struct net_if *iface)
{
    if (mgmt_event != NET_EVENT_DNS_SERVER_ADD)
    {
        return;
    }
    printk("DNS ready\n");
    s_isNetworkReady = true;
}

static void iface_up_evt_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
                    struct net_if *iface)
{
    if (mgmt_event != NET_EVENT_IF_UP)
    {
        return;
    }
    printk("interface is up\n");
}

static void iface_down_evt_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
                    struct net_if *iface)
{
    if (mgmt_event != NET_EVENT_IF_DOWN)
    {
        return;
    }
    printk("Interface is down\n");
    s_isNetworkReady = false;
}

static struct mgmt_events {
    uint32_t event;
    net_mgmt_event_handler_t handler;
    struct net_mgmt_event_callback cb;
} iface_events[] = {
    {.event = NET_EVENT_DNS_SERVER_ADD, .handler = iface_dns_added_evt_handler},
    {.event = NET_EVENT_IF_UP,          .handler = iface_up_evt_handler},
    {.event = NET_EVENT_IF_DOWN,        .handler = iface_down_evt_handler},
    {0} /* setup_iface_events requires this extra location. */
};

static void setup_iface_events(void)
{
    int i;
    for (i = 0; iface_events[i].event; i++)
    {
        net_mgmt_init_event_callback(&iface_events[i].cb, iface_events[i].handler,
                            iface_events[i].event);

        net_mgmt_add_event_callback(&iface_events[i].cb);
    }
}

K_SEM_DEFINE(lteConnectedSem, 0, 1);


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

// TODO this happens in the mqtteventhandler
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

//TODO fit current conventions (status, initialize etc)
static int tls_setup_http(int sock)
{
	int err;
	int verify;

	/* Security tag that we have provisioned the certificate with */
	const sec_tag_t tls_sec_tag[] = {
		CA_CERTIFICATE_TAG,
	};

	/* Set up TLS peer verification */
	verify = TLS_PEER_VERIFICATION_NONE; 

	err = setsockopt(sock, SOL_TLS, TLS_PEER_VERIFY, &verify, sizeof(verify));
	if (err)
    {
        printk("Failed to setup peer verification, err %d\n", errno);
        return err;
	}

	/* Associate the socket with the security tag
	 * we have provisioned the certificate with.
	 */
	err = setsockopt(sock, SOL_TLS, TLS_SEC_TAG_LIST, tls_sec_tag,
			 sizeof(tls_sec_tag));
	if (err)
    {
        printk("Failed to setup TLS sec tag, err %d\n", errno);
        return err;
	}

    err = setsockopt(sock, SOL_TLS, TLS_HOSTNAME, SHORT_URL, strlen(SHORT_URL));
	if (err)
    {
        printk("Failed to setup TLS_HOSTNAME, err %d\n", errno);
        return err;
	}

    /* TLS-ECDHE-RSA-WITH-AES-128-CBC-SHA256: 0xC027 */
    // int cipher_list[] = {0xC027};

    // err = setsockopt(sock, SOL_TLS, TLS_CIPHERSUITE_LIST, cipher_list, sizeof(cipher_list));
    // if (err) {
    //     printk("Failed to set up cipher suite list, err %d\n", errno);
    //     return err;
    // }

    int sessioncache = TLS_SESSION_CACHE_DISABLED;
    err = setsockopt(sock, SOL_TLS, TLS_SESSION_CACHE, &sessioncache, sizeof(sessioncache)); // done in lte
    printk("fd=%d with Security Tag Id %d is ready\n",  sock, CA_CERTIFICATE_TAG);	
	if (err)
    {
        printk("Failed to setup TLS_HOSTNAME, err %d\n", errno);
        return err;
	}

	return 0;
}

static void dump_addrinfo(const struct addrinfo *ai)
{
	printf("addrinfo @%p: ai_family=%d, ai_socktype=%d, ai_protocol=%d, "
	       "sa_family=%d, sin_port=%x\n",
	       ai, ai->ai_family, ai->ai_socktype, ai->ai_protocol,
	       ai->ai_addr->sa_family,
	       ((struct sockaddr_in *)ai->ai_addr)->sin_port);
}

typedef struct{
    char urlExt[MAX_URL_EXT_SIZE];
    int contentLen;
    char authToken[AUTH_HEADER_SIZE];
    char bodyGateWay[SIZE_OF_ADDRESS_BUFFER];
}setConnGWParams;

typedef struct{
    char *userCode;
    char *deviceCode;
}userDevCodes;

typedef struct{
    Token *accessToken;
    uint32_t *accessTokenExpiry;
}accessTokenInfo;

typedef enum{
    GET_ACCESS_TOKEN,
    SET_CONNECTION_GW,
    GET_CONNECTION_CODES,
    REGISTER_CONNECTION_GW,
    VALIDATE_AND_GET_REFRESH_TOKEN,
    UPDATE_ACCESS_TOKEN,
    REQUEST_GOAL_NUM,
}eRequestGoal;

static int setRequestBuffer(eHttpMsgType msgType, eRequestGoal goal, void *requestParams) //TODO improve this to an array for better runtime
{
    int status;
    setConnGWParams *setParams = requestParams;
    regAndRefreshParam *regParams = requestParams;

    switch (goal)
    {
        case GET_ACCESS_TOKEN:
            assert(requestParams == NULL);
            status = sprintf(s_requestBuffer, GET_TOKEN_REQUEST);
            break;
        case SET_CONNECTION_GW:
            status = sprintf(s_requestBuffer, SET_GW_REQUEST, setParams->urlExt, setParams->contentLen, setParams->authToken,
                        setParams->bodyGateWay);
            break;
        case GET_CONNECTION_CODES:
            status = sprintf(s_requestBuffer, GET_CODES_REQUEST);
            break;
        case REGISTER_CONNECTION_GW:
            status = sprintf(s_requestBuffer, REGISTER_CONNECTION_REQUEST, regParams->bodyLen, regParams->body);
            break;
        case VALIDATE_AND_GET_REFRESH_TOKEN:
            status = sprintf(s_requestBuffer, VALIDATE_REFRESH_REQUEST, regParams->bodyLen, regParams->body);
            break;
        case UPDATE_ACCESS_TOKEN:
            status = sprintf(s_requestBuffer, UPDATE_TOKEN_REQUEST, (char *)requestParams);
            break;
    }

    if (status < 0)
    {
        printk("ERROR sprintf failed\n");
        return SDK_FAILURE;
    }
    printk("REQ:\n|%s|\n", s_requestBuffer);

    return SDK_SUCCESS;
}

//TODO freeaddrinfo needed?
static int sendHttpRequest(eHttpMsgType msgType, eRequestGoal msgGoal, void *requestParams)
{
    int sock = -1;
	int status = 0;
    int offset = 0;
    int len = 0;
    struct addrinfo *res = NULL;
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    printk("########## in open httpsocket\n");
    status = getaddrinfo(SHORT_URL, HTTPS_PORT, &hints, &res);
    if (status != 0)
    {
        printk("###ERROR: Unable to resolve address, status %d, errno %d\n", status, errno);
        return SDK_FAILURE;
    }
    dump_addrinfo(res);

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TLS_1_2);
    if (sock == -1)
    {
        printk("###ERROR: Invalid socket %d, errno %d\n", sock, errno);
        return SDK_FAILURE;
    }

    status = tls_setup_http(sock);
    if (status != SDK_SUCCESS)
    {
        close(sock);
        return SDK_FAILURE;
    }

    status = connect(sock, res->ai_addr, sizeof(struct sockaddr_in));
    if (status == -1)
    {
        printk("###ERROR: failed to connect, err %d\n", errno);
        close(sock);
        return SDK_FAILURE;
    }

    status = setRequestBuffer(msgType, msgGoal, requestParams);
    if (status == SDK_FAILURE)
    {
        printk("###ERROR: setting request buffer failed, err %d\n", errno);
        close(sock);
        return SDK_FAILURE;
    }

    len = strlen(s_requestBuffer);

    do {
        status = send(sock, &s_requestBuffer[offset], len - offset, 0);
        if (status < 0)
        {
            printk("###ERROR: send failed, err %d\n", errno);
            close(sock);
            return SDK_FAILURE;
        }
        offset += status;
    } while (offset < len);

	printk("Response:\n\n");
    offset = 0;
    len = sizeof(s_response) - 1;
    memset(s_response, 0, sizeof(s_response));

	do {
		status = recv(sock, &s_response[offset], len - offset, 0); // change it to something with timeout (SO_RCVTIMEO?)

		if (status < 0) {
			printk("Error reading response\n");
            close(sock);
			return SDK_FAILURE;
		}
        offset += status;

		if (strstr(s_response, "Connection: close") != NULL) {
			break;
		}

	} while (status != 0);
    printk("|%s|\n", s_response);

    (void)close(sock);
    sock = -1;
    if (res != NULL)
    {
        freeaddrinfo(res);
    }

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

static SDK_STAT getTokenInfo(accessTokenInfo *buffer)
{
    SDK_STAT status = SDK_FAILURE;
    cJSON * checkDataJson = cJSON_GetObjectItem(s_lastJson, "access_token");
    if(checkDataJson)
    {
        memcpy(s_accessToken, cJSON_GetStringValue(checkDataJson), SIZE_OF_ACCESS_TOKEN_FULL);
        *buffer->accessToken = s_accessToken;

        checkDataJson = cJSON_GetObjectItem(s_lastJson, "expires_in");
        if(checkDataJson)
        {
            *buffer->accessTokenExpiry = (uint32_t)cJSON_GetNumberValue(checkDataJson);
            status = SDK_SUCCESS;
        }
    }

    return status;
}

//TODO handle fails within func
static SDK_STAT getResponseInfo(eRequestGoal goal, void *buffer)
{
    SDK_STAT status = SDK_SUCCESS;
    char *jsonPos = strchr(s_response, '{');
    if (jsonPos == NULL)
    {
        printk("ERROR failed to find json within response");
        return SDK_FAILURE;
    }
    s_lastJson = cJSON_Parse(jsonPos);

    switch (goal)
    {
        case GET_ACCESS_TOKEN:
            cJSON *accessTokenJson = cJSON_GetObjectItem(s_lastJson, "access_token");
            memcpy(buffer, cJSON_GetStringValue(accessTokenJson), SIZE_OF_ACCESS_CONNECTION_TOKEN);
            break;
        case SET_CONNECTION_GW:
            cJSON *checkDataJson = cJSON_GetObjectItem(s_lastJson, "data");
            printk("Json response: %s\r\n",cJSON_GetStringValue(checkDataJson));
            break;
        case GET_CONNECTION_CODES:
            userDevCodes *codeBuffers = buffer;
            cJSON * codesJson = cJSON_GetObjectItem(s_lastJson, "device_code");
            memcpy(codeBuffers->deviceCode, cJSON_GetStringValue(codesJson), SIZE_OF_DEVICE_CODE);
            codesJson = cJSON_GetObjectItem(s_lastJson, "user_code");
            memcpy(codeBuffers->userCode, cJSON_GetStringValue(codesJson), SIZE_OF_USER_CODE);
            break;
        case REGISTER_CONNECTION_GW:
            break;
        case VALIDATE_AND_GET_REFRESH_TOKEN:
            *(bool *)buffer = verifyConnectionResponse();
            break;
        case UPDATE_ACCESS_TOKEN:
            accessTokenInfo *tokenBuffer = buffer;
            status = getTokenInfo(tokenBuffer);
            break;
        default:
            printk("WRN unrecognized request goal within getResponseInfo\n");
            break;
    }

	cJSON_Delete(s_lastJson);

    return status;
}

static SDK_STAT getConnectionAccessToken(char * accessConnectionToken)
{
	int status = sendHttpRequest(HTTP_MSG_TYPE_POST, GET_ACCESS_TOKEN, NULL); //was setconnectionurl
    if (status == SDK_FAILURE)
    {
        return SDK_FAILURE;
    }

    status = getResponseInfo(GET_ACCESS_TOKEN, accessConnectionToken);

    return status;
}


static SDK_STAT setConnectionGateway(char * authorizationHeader)
{
	SDK_STAT status = SDK_SUCCESS;
    setConnGWParams params = {0};
    URL_EXT_GATEWAY_OWN(params.urlExt);
	BODY_GATEWAY(params.bodyGateWay);
    params.contentLen = strlen(params.bodyGateWay);
    strcpy(params.authToken, authorizationHeader);

    status = sendHttpRequest(HTTP_MSG_TYPE_PUT, SET_CONNECTION_GW, &params);

    status = getResponseInfo(SET_CONNECTION_GW, NULL);

	return SDK_SUCCESS;
}

static SDK_STAT getConnectionCodes(char * userCode, char * deviceCode)
{
	SDK_STAT status = SDK_SUCCESS;
    userDevCodes codes = {userCode, deviceCode};

    status = sendHttpRequest(HTTP_MSG_TYPE_POST, GET_CONNECTION_CODES, NULL);
    if (status == SDK_FAILURE)
    {
        printk("ERROR sendHttpRequest failed\n");
        return SDK_FAILURE;
    }

    status = getResponseInfo(GET_CONNECTION_CODES, &codes);

	return status;
}

static SDK_STAT registerConnectionGateway(char * bodyUserCode)
{
	SDK_STAT status  = SDK_SUCCESS;
    regAndRefreshParam params = {0};
    params.bodyLen = strlen(bodyUserCode);
    params.body = bodyUserCode;

    status = sendHttpRequest(HTTP_MSG_TYPE_POST, REGISTER_CONNECTION_GW, &params);
    if (status == SDK_FAILURE)
    {
        printk("ERROR sendHttpRequest failed\n");
    }

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
		return true;
	}
	else
	{
		checkDataJson = cJSON_GetObjectItem(s_lastJson, "error");
		__ASSERT((checkDataJson),"cJSON_GetObjectItem unexpected fail");
		int validError = strcmp("authorization_pending",cJSON_GetStringValue(checkDataJson));
		__ASSERT((validError == 0),"cJSON_GetObjectItem unexpected fail");
		OsalSleep(TIME_TO_RETRY_TOKEN_RECEIVE);
		return false;
	}
}

static SDK_STAT getConnectionAccessAndRefreshToken(char * bodyDeviceCode)
{
	SDK_STAT status = SDK_SUCCESS;
	bool receivedResponse = false;
    regAndRefreshParam params = {0};
    params.bodyLen = strlen(bodyDeviceCode);
    params.body = bodyDeviceCode;

	while(!receivedResponse)
	{
        status = sendHttpRequest(HTTP_MSG_TYPE_POST, VALIDATE_AND_GET_REFRESH_TOKEN, &params);
        if (status == SDK_FAILURE)
        {
            printk("ERROR sendHttpRequest failed\n");
            return SDK_FAILURE;
        }
        status = getResponseInfo(VALIDATE_AND_GET_REFRESH_TOKEN, &receivedResponse);
	}

    printk("Response recieved, GW approved!\n");

	return SDK_SUCCESS;
}

void LteInit()
{
    int status = 0;

    printk("#######Setting up modem functionality\n");
    status = mdm_hl7800_set_functionality(HL7800_FUNCTIONALITY_FULL);
    if (status < 0)
    {
        printk("######modem set_func failure, err %d\n", status);
        mdm_hl7800_set_functionality(HL7800_FUNCTIONALITY_FULL); // TODO if fails reset
    }

    printk("######finished initializing modem\n");
}

static SDK_STAT writeTlsCertificate()
{
    int status = SDK_SUCCESS;

    status = tls_credential_delete(CA_CERTIFICATE_TAG, TLS_CREDENTIAL_CA_CERTIFICATE); //don't delete?
    if (status == -EACCES)
    {
        printk("Failed accesing security tags\n");
        return SDK_FAILURE;
    }
    k_msleep(100);
    status = tls_credential_add(CA_CERTIFICATE_TAG, TLS_CREDENTIAL_CA_CERTIFICATE, wlt_cert, sizeof(wlt_cert));
    if (status != SDK_SUCCESS)
    {
        printk("Failed adding certificate\n");
        return SDK_FAILURE;
    }

    return SDK_SUCCESS;
}

SDK_STAT NetworkInit()
{
    int status = SDK_SUCCESS;
	
	k_event_init(&s_responseEvent);

    setup_iface_events();

	status = ModemInit(modemReadCallback); /*the handler interrupr the send/recv system calls!*/
	__ASSERT((status == SDK_SUCCESS),"ModemInit internal fail");

    while (!s_isNetworkReady)
    {
        printk("Waiting for network to be ready...\n");
        k_sleep(K_SECONDS(5));
    }

    k_msleep(100);

    if (SDK_SUCCESS != UpdateConfGatewayId())
    {
        printk("Failed setting imei-based id\n");
    }

    status = writeTlsCertificate();
    k_msleep(100);

	return status;
}

bool IsNetworkAvailable()
{
	return s_isNetworkReady;
}

//TODO handle failures better. free memory, retry/reset.
SDK_STAT ConnectToNetwork()
{
	SDK_STAT status = SDK_SUCCESS;
	char bodyDeviceCode[SIZE_OF_ADDRESS_BUFFER + sizeof(BODY_REG_POST) + SIZE_OF_DEVICE_CODE + 1] = {0};
	char bodyUserCode[SIZE_OF_ADDRESS_BUFFER + sizeof(BODY_REG_POST)  + SIZE_OF_USER_CODE + 1] = {0};
	char * authorizationHeader = NULL;
	int authIndex = 0;
	int deviceIndex = 0;
	int userIndex = 0;

	authorizationHeader = (char*)OsalCalloc(AUTH_HEADER_SIZE);
	if(!authorizationHeader)
	{
        printk("Allocation failed!\n");
		return SDK_FAILURE;
	}

	authIndex += sprintf((authorizationHeader + authIndex),HEADER_AUTHORIZATION HEADER_SEPERATE CONNECTION_TOKEN_TYPE);
	authIndex += sprintf((authorizationHeader + authIndex)," ");

	status = getConnectionAccessToken(authorizationHeader + authIndex);//gets token with api_sec_key
    printk("getconnectionaccesstoken %d\n", status);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);
	status = setConnectionGateway(authorizationHeader);//pre-register gw
    printk("setconnectiongateway %d\n", status);
	RETURN_ON_FAIL(status, SDK_SUCCESS, status);

	userIndex += sprintf((bodyUserCode + userIndex),"%s",BODY_REG_USR_CODE_PRE(s_addressStringBuffer));
	deviceIndex += sprintf((bodyDeviceCode + deviceIndex),"%s",BODY_REG_DEV_CODE_PRE(s_addressStringBuffer));

	getConnectionCodes((bodyUserCode + userIndex), (bodyDeviceCode + deviceIndex));//get codes required for registration and refresh token

	userIndex += SIZE_OF_USER_CODE;
	deviceIndex += SIZE_OF_DEVICE_CODE;

	userIndex += sprintf((bodyUserCode + userIndex),BODY_REG_POST);
	deviceIndex += sprintf((bodyDeviceCode + deviceIndex),BODY_REG_POST);

	status = registerConnectionGateway(bodyUserCode);//register gw
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
    accessTokenInfo params = {accessToken, accessTokenExpiry};

	// s_isLTEConfigurationDone = false; this pauses mqtt handling by modemreadcb

	if(!refreshToken || !accessToken || !accessTokenExpiry)
	{
		return SDK_INVALID_PARAMS;
	}

    status = sendHttpRequest(HTTP_MSG_TYPE_POST, UPDATE_ACCESS_TOKEN, refreshToken);
    if (status == SDK_FAILURE)
    {
        printk("ERROR sendHttpRequest failed\n");
        return SDK_FAILURE;
    }
    status = getResponseInfo(UPDATE_ACCESS_TOKEN, &params);

    return status;
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

/* Buffers for MQTT client */
#define RX_BUF_SIZE      (2048)
#define TX_BUF_SIZE      (4096)
#define PAYLOAD_BUF_SIZE (1024)

#define MQTT_TOPIC_ID_MAX_SIZE              (64)
#define MQTT_USER_MAX_SIZE                  (64)
#define MQTT_CLIENT_ID_MAX_SIZE             (64)
#define MQTT_BROKER_URI_MAX_SIZE            (128)
#define MQTT_PASSOWRD_MAX_SIZE              (2048)

static uint8_t rx_buffer[RX_BUF_SIZE] = {0};
static uint8_t tx_buffer[TX_BUF_SIZE] = {0};
static uint8_t payload_buf[PAYLOAD_BUF_SIZE] = {0};

uint8_t *pub_data_topic_id;
uint8_t *pub_status_topic_id;
uint8_t *sub_update_topic_id;
uint8_t *mqtt_client_id;
uint8_t *mqtt_user;
uint8_t *mqtt_uri;
uint8_t *mqtt_pw;
uint32_t mqtt_port;

struct mqtt_utf8 username;
struct mqtt_utf8 password;

bool mqtt_sub_ready = false;

static struct mqtt_client client;
static struct sockaddr_storage broker;
static bool mqttConnected = false;
static bool topicSubbed = false;

static struct pollfd mqtt_fd;

K_SEM_DEFINE(mqttConnectedSem, 0, 1);
K_SEM_DEFINE(mqttPollStartSem, 0, 1);

static void mqttPollingThreadFunc();

OSAL_THREAD_CREATE(mqttPollingThread, mqttPollingThreadFunc, SIZE_OF_MQTT_POLLER_THREAD, THREAD_PRIORITY_HIGH);

static bool isMqttConnected(void) //TODO unify with isConnectedToBrokerServer
{
    return mqttConnected;
}

static void setMqttConnected(bool status)
{
    mqttConnected = status;
    eConnectionStates state = status ? CONN_STATE_CONNECTION : CONN_STATE_DISCONNECTION;
    if (status == true)
    {
        k_sem_give(&mqttConnectedSem);
    }
    else
    {
        k_sem_reset(&mqttConnectedSem);
    }
    s_receivedConnStateHandle(&state); //this is changeconnstatecallback
}

#define WAIT_FOR_MQTT_RECONNECT(semaphore)  ({                                  \
                                            k_sem_take(semaphore, K_FOREVER);   \
                                            continue;                           \
                                            })
//TODO error handling instead of breaks. perhaps a k_sem_take with continue.
static void mqttPollingThreadFunc()
{
    int rc;

    k_sem_take(&mqttPollStartSem, K_FOREVER);

    while (true)
    {
        rc = poll(&mqtt_fd, 1, mqtt_keepalive_time_left(&client));
        if (rc < 0) 
        {
            printk("Warning: poll error %d.\n", errno);
            if (!isMqttConnected() || errno == EBADF)
            {
                printk("Waiting for mqtt reconnection attempt..");
                WAIT_FOR_MQTT_RECONNECT(&mqttPollStartSem);
            }
        }
        else if (rc == 0) /* keepalive timeout reached */
        {
            rc = mqtt_live(&client);
            if ((rc != 0) && (rc != -EAGAIN))
            {
                printk("(%s) Warning: mqtt_live  failed rc = %d %s\n", (__FUNCTION__),  rc, 
                        rc == -ENOTCONN ? "(Socket is not connected)" : "");
            }
            continue;
        }

        if ((mqtt_fd.revents & POLLIN) == POLLIN)
        {
            rc = mqtt_input(&client);
            if (rc != 0)
            {
                printk("(%s) Warning: mqtt_input %d\n", (__FUNCTION__), rc);
            }
        }
        if ((mqtt_fd.revents & POLLERR) == POLLERR)
        {
            printk("(%s) POLLERR\n", (__FUNCTION__));
        }
        if ((mqtt_fd.revents & POLLNVAL) == POLLNVAL)
        {
            printk("(%s) POLLNVAL\n", (__FUNCTION__));
        }
    }
}

static bool isTopicSubbed(void)
{
    return topicSubbed;
}

static void setTopicSubbed(bool status)
{
    topicSubbed = status;
}

SDK_STAT SubscribeToTopic(char * topic)
{
    int status;
	struct mqtt_topic subscribe_topic = {
		.topic = {
			.utf8 = topic,
			.size = strlen(topic)
		},
		.qos = MQTT_QOS_0_AT_MOST_ONCE
	};
	const struct mqtt_subscription_list subscription_list = {
		.list = &subscribe_topic,
		.list_count = 1,
		.message_id = 1234
	};

    if(isTopicSubbed())
    {
        return SDK_SUCCESS;
    }
	printk("Subscribing to: %s len %u\n", topic, strlen(topic));

    status = mqtt_subscribe(&client, &subscription_list);
    if (status != 0)
    {
        printk("mqtt_subscribe FAILED, status %d\n", status);
        return SDK_FAILURE;
    }

    return SDK_SUCCESS;
}

conn_handle IsConnectedToBrokerServer()
{
    if (isMqttConnected() && isTopicSubbed())
    {
        return (conn_handle)1;
    }

    return NULL;
}

//TODO finalize it (make a reboot function first probably)
void mqttEventHandler(struct mqtt_client *client, const struct mqtt_evt *evt)
{
	int err;
    cJSON * cloud_config_json = NULL;

	switch (evt->type)
    {
        case MQTT_EVT_CONNACK:
            if (evt->result != 0)
            {
                printk("###Mqtt connection failed, %d\n", evt->result);
                OsalSystemReset();
                return;
            }
            setMqttConnected(true);
            SubscribeToTopic(sub_update_topic_id);
            break;

        case MQTT_EVT_DISCONNECT:
            setMqttConnected(false);
            setTopicSubbed(false);
            break;

        case MQTT_EVT_PUBLISH: 
            const int publishLen = evt->param.publish.message.payload.len;
            if (evt->result != 0 || publishLen > (PAYLOAD_BUF_SIZE - 1))
            {
                printk("###Mqtt publish failed. err %d, published message len > buffsize? %d\n", evt->result, 
                    publishLen > (PAYLOAD_BUF_SIZE - 1));
                OsalSystemReset();
                //TODO error handle, reboot?
                return;
            }
            err = mqtt_readall_publish_payload(client, payload_buf, publishLen);
            if (err < 0)
            {
                printk("Failed reading publish payload from subscribed topic, err %d\n", err);
                //TODO error handle, reboot?
                return;
            }
            payload_buf[publishLen] = '\0'; //necessary? probably yes
            if (!IS_JSON_PREFIX(payload_buf))
            {
                printk("Payload recieved is not identified as JSON(first char == %c\n", payload_buf[0]);
                return;
            }
            s_receivedMQTTPacketHandle(payload_buf, publishLen);
            break;

        case MQTT_EVT_PUBACK:
            if (evt->result != 0)
            {
                printk("###Mqtt puback failed, %d\n", evt->result);
                //TODO error handle, reboot?
                return;
            }
            break;

        case MQTT_EVT_SUBACK:
            if (evt->result != 0)
            {
                printk("###Mqtt suback failed, %d\n", evt->result);
                //TODO error handle, reboot?
                return;
            }
            setTopicSubbed(true);
            break;

        case MQTT_EVT_PINGRESP:
            if (evt->result != 0)
            {
                printk("###Mqtt ping response failed, %d\n", evt->result);
                //TODO error handle, reboot?
                return;
            }
            // printk("##Got ping response\n");
            break;

        case MQTT_EVT_UNSUBACK: 
            if (evt->result != 0)
            {
                printk("###Mqtt unsuback failed, %d\n", evt->result);
                //TODO error handle, reboot?
                return;
            }
            printk("Mqtt unsubscribed successfully\n");
            setTopicSubbed(false);
            break;

        default:
            printk("[%s:%d] Unsupported Event Type: %d", __func__, __LINE__, evt->type);
            break;
	}
}

static int setMqttCredentials(void)
{
    const char *gatewayId = NULL;
    SDK_STAT status = SDK_SUCCESS;

    status = GetGatewayId(&mqtt_client_id);
    if (status != SDK_SUCCESS)
    {
        printk("GetGatewayId FAILED, status %d\n", status);
        return status;
    }

    status = GetAccountID((const char **)&mqtt_user);
    if (status != SDK_SUCCESS)
    {
        printk("GetAccountID FAILED, status %d\n", status);
        return status;
    }

    status = GetAccessToken((Token *)&mqtt_pw);
    if (status != SDK_SUCCESS)
    {
        printk("GetAccessToken FAILED, status %d\n", status);
        return status;
    }

    status = GetMqttServer(&mqtt_uri);
    if (status != SDK_SUCCESS)
    {
        printk("GetMqttServer FAILED, status %d\n", status);
        return status;
    }

    mqtt_port = MQTT_SERV_PORT;

    printk("MQTT credentials:\n#id:%s\n#user:%s\n#pw:%s\n#uri:%s\n#port:%u\n", mqtt_client_id, mqtt_user, mqtt_pw, mqtt_uri, mqtt_port);

    return SDK_SUCCESS;
}

static int setMqttTopics(void)
{
    pub_data_topic_id = GetMqttDownlinkTopic();
    pub_status_topic_id = GetMqttStatusTopic();
    sub_update_topic_id = GetMqttUplinkTopic();

    printk("MQTT Publish Data Topic = %s (%d Bytes)\n", (pub_data_topic_id), strlen(pub_data_topic_id));
    printk("MQTT Publish Status Topic = %s (%d Bytes)\n", (pub_status_topic_id), strlen(pub_status_topic_id));
    printk("MQTT Subscribe Update Topic = %s (%d Bytes)\n", (sub_update_topic_id), strlen(sub_update_topic_id));
    printk("MQTT Client Id = %s (%d Bytes)\n", (mqtt_client_id), strlen(mqtt_client_id));

    return SDK_SUCCESS;
}

static SDK_STAT brokerInit(void)
{
	int err;
	struct addrinfo *result = NULL;
	struct addrinfo *addr = NULL;
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM
	};

	err = getaddrinfo(mqtt_uri, NULL, &hints, &result);
	if (err != 0)
    {
		printk("ERROR: getaddrinfo failed %d (errno=%d)", err, errno);
		return SDK_FAILURE;
	}

	addr = result;

	/* Look for address of the broker. */
	while (addr != NULL)
    {
		/* IPv4 Address. */
		if (addr->ai_addrlen == sizeof(struct sockaddr_in))
        {
			struct sockaddr_in *broker4 = (struct sockaddr_in *)&broker;
			char ipv4Addr[NET_IPV4_ADDR_LEN];

			broker4->sin_addr.s_addr = ((struct sockaddr_in *)addr->ai_addr)->sin_addr.s_addr;
			broker4->sin_family = AF_INET;
			broker4->sin_port = htons(mqtt_port);

			inet_ntop(AF_INET, &broker4->sin_addr.s_addr,
				  ipv4Addr, sizeof(ipv4Addr));
			printk("MQTT Broker IPv4 Address is %s", (ipv4Addr));
			break;
		} 
        else 
        {
			printk("ERROR ai_addrlen = %u should be %u or %u",
				(unsigned int)addr->ai_addrlen,
				(unsigned int)sizeof(struct sockaddr_in),
				(unsigned int)sizeof(struct sockaddr_in6));
		}

		addr = addr->ai_next;
	}

	/* Free the address. */
    if(result != NULL)
    {
        freeaddrinfo(result);
    }

	return SDK_SUCCESS;
}

#define CONFIG_SEC_TAG (51966)
static sec_tag_t secTagList[] = { CA_CERTIFICATE_TAG };

static SDK_STAT clientInit(void)
{
    SDK_STAT status;
    username.utf8 = mqtt_user;
    username.size = strlen(mqtt_user);
    password.utf8 = mqtt_pw;
    password.size = strlen(mqtt_pw);        

	mqtt_client_init(&client);

	status = brokerInit();
	if (status != SDK_SUCCESS)
    {
		printk("Failed to initialize broker connection\n");
		return SDK_FAILURE;
	}

   	/* MQTT client configuration */
	client.broker = &broker;
	client.evt_cb = mqttEventHandler;
	client.client_id.utf8 = mqtt_client_id;
	client.client_id.size = strlen(mqtt_client_id);
	client.password = &password;
    client.user_name = &username;        
	client.protocol_version = MQTT_VERSION_3_1_1;
    client.keepalive = 120; // 2 minutes
	client.transport.type = MQTT_TRANSPORT_SECURE;

	/* MQTT buffers configuration */
	client.rx_buf = rx_buffer;
	client.rx_buf_size = RX_BUF_SIZE;
	client.tx_buf = tx_buffer;
	client.tx_buf_size = TX_BUF_SIZE;

	/* MQTT transport configuration */
	struct mqtt_sec_config *tlsConfig = &client.transport.tls.config;

	tlsConfig->peer_verify = TLS_PEER_VERIFICATION_NONE; //TODO this is less secured
	tlsConfig->cipher_count = 0;
	tlsConfig->cipher_list = NULL;
	tlsConfig->sec_tag_count = ARRAY_SIZE(secTagList);
	tlsConfig->sec_tag_list = secTagList;
	tlsConfig->hostname = NULL;
	tlsConfig->session_cache = TLS_SESSION_CACHE_DISABLED;
       
	return SDK_SUCCESS;
}

static void setMqttFd(void)
{
    mqtt_fd.fd = client.transport.tls.sock;
    mqtt_fd.events = POLLIN;
}

static SDK_STAT mqttInit(void)
{
    SDK_STAT status;

    status = setMqttCredentials();
    if (status != SDK_SUCCESS)
    {
        printk("setMqttCredentials failed, status %d\n", status);
        return status;
    }
    
    status = setMqttTopics();
    if (status != SDK_SUCCESS)
    {
        printk("setMqttTopics failed, status %d\n", status);
        return status;
    }

    status = clientInit();
    if (status != SDK_SUCCESS)
    {
        printk("ClientInit failed, status %d\n", status);
        return status;
    }

    return SDK_SUCCESS;
}

static SDK_STAT waitForMqttConnection()
{
    int status;

    status = k_sem_take(&mqttConnectedSem, K_MSEC(MQTT_CONNECT_TIMEOUT_MS));
    if (status != 0)
    {
        printk("ERROR Mqtt connection was not acknowledged\n");
        return SDK_FAILURE;
    }

    printk("MQTT connected successfully!\n");

    return SDK_SUCCESS;
}

static SDK_STAT attemptMqttConnection()
{
    int rc = 0;
    int attempts = 0;

    rc = mqtt_connect(&client);
    while (rc == -EIO && attempts < 5)
    {
        printk("mqtt connection failed, retrying...\n");
        k_msleep(5000);
        rc = mqtt_connect(&client);
        ++attempts;
    }
    
    if (rc != 0)
    {
        printk("mqtt_connect failed, err %d errno %d\n", rc, errno);
        return SDK_FAILURE;
    }

    return SDK_SUCCESS;
}

conn_handle ConnectToServer()
{
	int rc = SDK_SUCCESS;

    rc = mqttInit();
    if (rc != SDK_SUCCESS)
    {
        printk("mqttInit failed\n");
        return NULL;
    }

    if (SDK_SUCCESS != attemptMqttConnection())
    {
        printk("Too many mqtt connection attempts failed, rebooting\n");
        OsalSystemReset();
        return NULL;
    }

    setMqttFd();
    k_sem_give(&mqttPollStartSem);

    rc = waitForMqttConnection();
    if (rc != SDK_SUCCESS)
    {
        printk("ERROR Mqtt could not connect, rebooting\n");
        OsalSystemReset();
        return NULL;
    }

	s_connHandle = (conn_handle)CONNECTION_HANDLE; //prev imp

	return s_connHandle;
}

SDK_STAT NetSendMQTTPacket(const char* topic, void* pkt, uint32_t length)
{
	SDK_STAT status = SDK_SUCCESS;
    struct mqtt_publish_param param = {0};

	if(!pkt || !topic || length == 0)
	{
		return SDK_INVALID_PARAMS;
	}

    param.message.topic.qos = MQTT_PACKET_DEFAULT_QOS;
    param.message.topic.topic.utf8 = topic;
    param.message.topic.topic.size = strlen(topic);
    param.message.payload.data = pkt;
    param.message.payload.len = length;

    status = mqtt_publish(&client, &param);
    if (status != 0)
    {
        printk("mqtt_publish failed, status %d, errno %d\n", status, errno);
        return SDK_FAILURE;
    }

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
    int attempts = 0;
    SDK_STAT status;

    status = mdm_hl7800_reset();
    if (status != 0)
    {
        printk("Failed resetting modem\n");
    }

    while (!s_isNetworkReady)
    {
        printk("Waiting for network to be ready...\n");
        k_sleep(K_SECONDS(5));
    }

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
    return mdm_hl7800_get_imei();
}
