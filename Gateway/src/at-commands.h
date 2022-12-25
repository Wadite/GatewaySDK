#ifndef _AT_COMMANDS_H
#define _AT_COMMANDS_H

#include "at-commands-libary.h"
#include "modem-drv.h"

#include <stdio.h>
#include <stdint.h>

typedef enum{
    AT_CMD_TEST,
    AT_CMD_QCCID,
    AT_CMD_CTZR,
    AT_CMD_CTZU,
    AT_CMD_QCFG,
    AT_CMD_CGDCONT,
    AT_CMD_CEREG,
    AT_CMD_COPS,
    AT_CMD_QCSQ,
    AT_CMD_QLTS,
    AT_CMD_QPING,
    AT_CMD_QHTTPCFG,
    AT_CMD_QHTTPURL,
    AT_CMD_QHTTPPOST,
    AT_CMD_QHTTPGET,
    AT_CMD_QHTTPPUT,
    AT_CMD_QHTTPREAD,

    AT_CMD_NUM
} eAtCmds;

typedef union {
    AtCtzrParams    atCtzrParams;
    AtCtzuParams    atCtzuParams;
    AtQcfgParams    atQcfgParams;
    AtCgdcontParams atCgdcontParams;
    AtCereg         atCereg;
    AtQlts          atQlts;
    AtQping         atQping;
    AtQhttpcfg      atQhttpcfg;
    AtQhttpurl      atQhttpurl;
    AtQhttppost     atQhttppost;
}AtCmndsParams;

/**
 * @brief AT type test command function
 * 
 * @return SDK_SUCCESS if command was sent succefull
 */
SDK_STAT AtTestCmd();

/**
 * @brief AT type read command function
 * 
 * @param atCmd enum of requested command
 *
 * @return SDK_INVALID_PARAMS if enum not in bounds.
 * @return SDK_FAILURE if internal bug occurred(higher level should assert).
 * @return SDK_SUCCESS if command was sent succefully.
 */
SDK_STAT AtReadCmd(const eAtCmds atCmd);

/**
 * @brief AT type write command function
 * 
 * @param atCmd enum of requested command
 * @param cmndParams pointer to parameters of command
 *
 * @return SDK_INVALID_PARAMS if enum not in bounds or null pointer.
 * @return SDK_FAILURE if internal bug occurred(higher level should assert).
 * @return SDK_SUCCESS if command was sent succefully.
 */
SDK_STAT AtWriteCmd(const eAtCmds atCmd, const AtCmndsParams * cmndParams);

/**
 * @brief AT type execution command function
 * 
 * @param atCmd enum of requested command
 *
 * @return SDK_INVALID_PARAMS if enum not in bounds.
 * @return SDK_FAILURE if internal bug occurred(higher level should assert).
 * @return SDK_SUCCESS if command was sent succefully.
 */
SDK_STAT AtExecuteCmd(const eAtCmds atCmd);

// add description please

SDK_STAT atEnumToResponseString(eAtCmds atCmd, char * responseBuffer, uint16_t bufferSize);

#endif //_AT_COMMANDS_H