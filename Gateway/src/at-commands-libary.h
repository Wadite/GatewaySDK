#ifndef _AT_COMMANDS_LIBARY_H
#define _AT_COMMANDS_LIBARY_H

typedef enum{
    MODE_DISABLE_TIME_ZONE_CHANGE           = 0,
    MODE_ENABLE_TIME_ZONE_CHANGE            = 1,
    MODE_ENABLE_EXTENDED_TIME_ZONE_CHANGE   = 2
} eModeOfTimeZoneReporting;

typedef enum{
    MODE_DISABLE_AUTOMATIC_TIME_ZONE_UPDATE                         = 0,
    MODE_ENABLE_AUTOMATIC_TIME_ZONE_UPDATE                          = 1,
    MODE_ENABLE_AUTOMATIC_TIME_ZONE_UPDATE_AND_UPDATE_LOCAL_TIME    = 3
} eModeOfAutomaticTimeZoneUpdate;

typedef enum{
    EXTENDED_CONFIGURE_RATS_SEARCHING_SEQUENCE,
    EXTENDED_CONFIGURE_RATS_TO_BE_SEARCHED,
    EXTENDED_CONFIGURE_NETWORK_SEARCH_UNDER_LTE_RAT,

    EXTENDED_CONFIGURE_NUM
} eExtendedConfigurationCommands;

typedef enum{
    PDP_TYPE_IPV4,
    PDP_TYPE_PPP,
    PDP_TYPE_IPV6,
    PDP_TYPE_IPV4V6,

    PDP_TYPE_NUM
} ePdpType;

typedef enum{
    HTTP_CONFIGURATION_PDP_CONTEXT_ID,
    HTTP_CONFIGURATION_REQUEST_HEADER,
    HTTP_CONFIGURATION_RESPOSNSE_HEADER,
    HTTP_CONFIGURATION_CONTENT_TYPE,

    HTTP_CONFIGURATION_NUM
} eHttpConfigurationCommands;

typedef enum{
    NETWORK_REGISTRATION_DISABLE = 0,
    NETWORK_REGISTRATION_ENABLE = 1,
    NETWORK_REGISTRATION_AND_LOCATION_INFORMATION_ENABLE = 2,
    NETWORK_REGISTRATION_AND_LOCATION_INFORMATION_UE_ENABLE = 4,
}eNetworkRegistrationStatus;

typedef enum{
    QUERY_LATEST_TIME_SYNCHRONIZED_THROUGH_NETWORK = 0,
    QUERY_THE_CURRENT_GMT_TIME = 1,
    QUERY_THE_CURRENT_LOCAL_TIME = 2,
}eQueryNetworkTimeMode;

typedef struct{
    eModeOfTimeZoneReporting mode;
}AtCtzrParams;

typedef struct{
    eModeOfAutomaticTimeZoneUpdate mode;
}AtCtzuParams;

typedef struct{
    eExtendedConfigurationCommands cmd;
    char * mode;
    int effect;
}AtQcfgParams;

typedef struct{
    int cid;
    ePdpType type;
    char * apn;
}AtCgdcontParams;

typedef struct{
    eNetworkRegistrationStatus status;
}AtCereg;

typedef struct{
    eQueryNetworkTimeMode mode;
}AtQlts;

typedef struct{
    int contextId;
    char * host;
}AtQping;

typedef struct{
    eHttpConfigurationCommands cmd;
    int value;
}AtQhttpcfg;

typedef struct{
    int urlLength;
    int urlTimeout;
}AtQhttpurl;

typedef struct{
    int postBodySize;
    int inputTimeout;
    int responseTimeout;
}AtQhttppost;

#endif //_AT_COMMANDS_LIBARY_H