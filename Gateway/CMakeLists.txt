# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(scan_adv)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

add_definitions(-DDEBUG)

zephyr_library_include_directories(${ZEPHYR_BASE}/samples/bluetooth)

zephyr_library_include_directories("../sdk/Interface")
zephyr_library_include_directories("../sdk/sdkInc")
zephyr_library_include_directories("src")

target_sources(app PRIVATE src/osal/osal.c)
target_sources(app PRIVATE src/dev-if/dev-if.c)
target_sources(app PRIVATE src/modem-drv.c)
target_sources(app PRIVATE src/network-api.c)
target_sources(app PRIVATE src/at-commands.c)
target_sources(app PRIVATE src/httpStringBuilder.c)
target_sources(app PRIVATE ../sdk/sdkSrc/cJSON.c)
target_sources(app PRIVATE ../sdk/sdkSrc/logger.c)
target_sources(app PRIVATE ../sdk/sdkSrc/williotSdkJson.c)
target_sources(app PRIVATE ../sdk/sdkSrc/upLink.c)