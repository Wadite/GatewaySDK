Wiliot Gateway SDK - Usage Guide
================================

This guide provides instructions for developers on how to use the Wiliot Gateway SDK, the available APIs, and steps to initialize, build, and test the SDK.

Table of Contents
-----------------

-   [Initialization]
-   [APIs]
-   [Building]
-   [Testing]

Initialization
--------------

To initialize the configuration module, call the `ConfigurationInit` function. This function reads the existing configuration parameters from storage, or loads the default values if none are found.



`SDK_STAT ConfigurationInit();`

APIs
----

The following APIs are available in the Wiliot Gateway SDK:

1.  `SetConfiguration`

    Set the configuration parameters using a cJSON object.



    `SDK_STAT SetConfiguration(const cJSON *item);`

2.  `GetConfigurationJsonString`

    Get the current configuration as a JSON string.



    `static const char * getConfigurationJsonString();`

3.  `SendConfigurationToServer`

    Send the current configuration to the server via MQTT.



    `SDK_STAT SendConfigurationToServer();`

4.  `GetConfigurationKeyName`

    Get the key name for a specific configuration parameter.



    `const char * GetConfigurationKeyName(eConfigurationParams confParm);`

5.  `Configuration` getters:

    Various getter functions are available for each configuration parameter, such as:

    -   `GetLoggerUploadMode`
    -   `GetLoggerSeverity`
    -   `GetLoggerLocalTraceConfig`
    -   `GetLoggerNumberOfLogs`
    -   `GetUuidToFilter`
    -   `GetAccountID`
    -   `GetGatewayType`
    -   `GetGatewayId`
    -   `GetIsLocationSupported`
    -   `GetLatitude`
    -   `GetLongitude`
    -   `GetMqttServer`
    -   `GetApiVersion`

Building
--------

To build the Wiliot Gateway SDK, follow these steps:

1.  Clone the repository to your local machine.

2.  Install the required tools and libraries for your platform.

3.  Configure your build environment according to the platform-specific instructions.

4.  Build the SDK by running the appropriate build command for your platform.

Testing
-------

To test the Wiliot Gateway SDK, follow these steps:

1.  Connect your gateway hardware to your development machine.

2.  Build and run the SDK on the gateway hardware.

3.  Check the output for any errors or issues.

4.  Use the available APIs to interact with the SDK and test the expected behavior.

5.  Verify that the gateway is able to communicate with the Wiliot cloud and send events.

If you encounter any issues or need further assistance, consult the Wiliot documentation or reach out to the Wiliot support team.
