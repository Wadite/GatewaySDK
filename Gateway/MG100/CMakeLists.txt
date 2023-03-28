# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(wiliot_gateway)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

add_definitions(-DDEBUG)

zephyr_library_include_directories("../../sdk/Interface")
zephyr_library_include_directories("../../sdk/sdkInc")
zephyr_library_include_directories("src")

target_sources(app PRIVATE src/osal/osal.c)
target_sources(app PRIVATE src/dev-if/dev-if.c)
target_sources(app PRIVATE src/modem-drv.c)
target_sources(app PRIVATE src/network-api.c)
target_sources(app PRIVATE src/flash-drv.c)
target_sources(app PRIVATE ../../sdk/sdkSrc/cJSON.c)
target_sources(app PRIVATE ../../sdk/sdkSrc/logger.c)
target_sources(app PRIVATE ../../sdk/sdkSrc/williotSdkJson.c)
target_sources(app PRIVATE ../../sdk/sdkSrc/upLink.c)
target_sources(app PRIVATE ../../sdk/sdkSrc/downLink.c)
target_sources(app PRIVATE ../../sdk/sdkSrc/sdkConfigurations.c)
target_sources(app PRIVATE ../../sdk/sdkSrc/mqttTopics.c)
target_sources(app PRIVATE ../../sdk/sdkSrc/networkManager.c)
target_sources(app PRIVATE ../../sdk/sdkSrc/serviceDiscovery.c)
