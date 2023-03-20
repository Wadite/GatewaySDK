#include "modem-drv.h"
#include "osal.h"

#include <assert.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/modem/hl7800.h>

#include "logger.h"

#define PIN_CATM_EN                                         (25)
#define PIN_PWRKET_MCU                                      (26)

#define KERNEL_STACK_SIZE                                   (4096)
#define UART_RX_MSGQ_SIZE                                   (128)
#define UART_TX_MSGQ_SIZE                                   (128)
#define PROTOCOL_MAX_MSG_SIZE                               (2048)
#define INITAL_COUNT                                        (0)
#define COUNT_LIMIT                                         (1)

#define INPUT_STRING_FIRST                                  '>'
#define INPUT_STRING_SECOND                                 ' '
#define SIZE_OF_INPUT_STRING                                (2)
#define IS_NORMAL_MSG_RECEIVED(msgBuf,msgSize)              ((msgBuf)[(msgSize) - 1] == '\n') 
#define IS_INPUT_MSG_RECEIVED(msgBuf,msgSize)               (((msgSize) == SIZE_OF_INPUT_STRING) && \
                                                                ((msgBuf)[(msgSize) - 2] == INPUT_STRING_FIRST) && \
                                                                ((msgBuf)[(msgSize) - 1] == INPUT_STRING_SECOND))
#define IS_MSG_TOO_LONG(msgSize)                            ((msgSize) == PROTOCOL_MAX_MSG_SIZE)


static const struct device* s_uartDev;
static struct k_thread s_uartReadThread;
static K_KERNEL_STACK_DEFINE(s_uartReadThreadStack, KERNEL_STACK_SIZE);
static K_SEM_DEFINE(s_txSendSem, INITAL_COUNT, COUNT_LIMIT);

static uint8_t s_uartRxMsgqBuf[UART_RX_MSGQ_SIZE];
static struct k_msgq s_uartRxMsgq;

static uint8_t* s_uartTxMsgqBuf;
static uint16_t s_txBufSize = 0;
static ReadModemDataCB s_modemReadFunc = NULL;

static void processRead()
{
    uint8_t byte = 0;
    int bytesRead = 0;

    // Copy all FIFO contents to the message queue
    do {
        bytesRead = uart_fifo_read(s_uartDev, &byte, 1);
        __ASSERT((bytesRead >= 0),"Received error from uart_fifo_read");

        if (bytesRead > 0)
        {
            // remove print after finish testing
            k_msgq_put(&s_uartRxMsgq, &byte, K_NO_WAIT);
        }
    } while (bytesRead > 0);
}

static void processSend()
{
    int bytesSent = 0;
    static int s_bytesOffset = 0;

    bytesSent = uart_fifo_fill(s_uartDev, s_uartTxMsgqBuf + s_bytesOffset, s_txBufSize);
    __ASSERT((bytesSent >= 0),"Received error from uart_fifo_fill");

    if(bytesSent < s_txBufSize)
    {
        s_bytesOffset += bytesSent;
        s_txBufSize -= bytesSent;                
    }
    else
    {
        s_bytesOffset = 0;
        uart_irq_tx_disable(s_uartDev);
        k_sem_give(&s_txSendSem);
    }
}

static void modemUartIsr(const struct device* dev, void* user_data) 
{
    ARG_UNUSED(dev);
    ARG_UNUSED(user_data);

    while (uart_irq_update(dev) && uart_irq_is_pending(dev)) 
    {
        if (uart_irq_tx_ready(dev)) 
        {
			processSend();
		}
		if (uart_irq_rx_ready(dev)) 
        {
			processRead();
		}
	}
}

static inline void processMessage(uint8_t* buf, int size) 
{
    __ASSERT(s_modemReadFunc, "Modem read func doesn't exist");
    s_modemReadFunc(buf, size);
}

static void uartRead(void* p1, void* p2, void* p3) 
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    static uint8_t s_msgBuf[PROTOCOL_MAX_MSG_SIZE + 1];
    
    int msgSize = 0;
    int result = 0;
    LOG_DEBUG_INTERNAL("uartRead thread running\n");
    
    while (true) {
        result = k_msgq_get(&s_uartRxMsgq, &(s_msgBuf[msgSize]), K_FOREVER);
        __ASSERT((result >= 0),"Received error from k_msgq_get");

        msgSize++;

        // Normal command
        if(IS_NORMAL_MSG_RECEIVED(s_msgBuf,msgSize)) 
        {
            s_msgBuf[msgSize-1] = '\0';
            s_msgBuf[msgSize-2] = '\0';
            processMessage(s_msgBuf, msgSize - 1);
            msgSize = 0;
        }

        // Input string command
        else if(IS_INPUT_MSG_RECEIVED(s_msgBuf,msgSize))
        {
            s_msgBuf[msgSize] = '\0';
            processMessage(s_msgBuf, msgSize);
            msgSize = 0;
        }

        // Msg too long
        else if(IS_MSG_TOO_LONG(msgSize)) 
        {
            LOG_DEBUG_INTERNAL("Malformed s_uartDev message: too large\n");
            msgSize = 0;
        }
    }
    
}

SDK_STAT ModemSend(const void* dataBuff, uint16_t buffSize)
{
    int err = 0;

    if(!dataBuff)
    {
        return SDK_INVALID_PARAMS;
    }

    s_uartTxMsgqBuf = (uint8_t*)dataBuff;
    s_txBufSize = buffSize;
	uart_irq_tx_enable(s_uartDev);

    err = k_sem_take(&s_txSendSem, K_FOREVER);
    printk("Sent: |%s|\n", s_uartTxMsgqBuf);
    if(err)
    {
        return SDK_FAILURE;
    }

    return SDK_SUCCESS;
}

extern struct k_sem lteConnectedSem;
static int i = 0;

void modemCallback(enum mdm_hl7800_event event, void *event_data)
{
    struct mdm_hl7800_compound_event *net_change;
    ++i;
    switch (event)
    {
        case HL7800_EVENT_NETWORK_STATE_CHANGE:
            net_change = event_data;
            if (net_change->code == HL7800_HOME_NETWORK || net_change->code == HL7800_ROAMING)
            {
                k_sem_give(&lteConnectedSem);
            }
            break;
        case HL7800_STARTUP_STATE_UNRECOVERABLE_ERROR:
            //TODO reset
            break;
    }
}

void printI()
{
    printk("cb i %d\n", i);
}

struct mdm_hl7800_callback_agent agent;

static void modemDeviceStart()
{
    int status = 0;

    printk("Starting modem\n");

    status = mdm_hl7800_reset();
    if (status != 0)
    {
        printk("########failed resetting modem\n");
    }
}


SDK_STAT ModemInit(ReadModemDataCB cb) 
{
    modemDeviceStart();

    return SDK_SUCCESS;
}

void modemUnregCb()
{
    mdm_hl7800_unregister_event_callback(&agent);
}