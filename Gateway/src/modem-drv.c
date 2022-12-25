#include "modem-drv.h"
// #include "util.h"
// #include "tg_assert.h"

#include <zephyr/zephyr.h>
#include <assert.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>

#define PIN_CATM_EN             (25)
#define PIN_PWRKET_MCU          (26)

#define KERNEL_STACK_SIZE       (4096)
#define UART_RX_MSGQ_SIZE       (128)
#define UART_TX_MSGQ_SIZE       (128)
#define PROTOCOL_MAX_MSG_SIZE   (2048)
#define INITAL_COUNT            (0)
#define COUNT_LIMIT             (1)

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
    printk("uartRead thread running\n");

    while (true) {
        result = k_msgq_get(&s_uartRxMsgq, &(s_msgBuf[msgSize]), K_FOREVER);
        __ASSERT((result >= 0),"Received error from k_msgq_get");

        msgSize++;

        if (s_msgBuf[msgSize - 1] == '\n') 
        {
            s_msgBuf[msgSize-1] = '\0';
            s_msgBuf[msgSize-2] = '\0';
            processMessage(s_msgBuf, msgSize - 1);
            msgSize = 0;
        }

        if (msgSize == PROTOCOL_MAX_MSG_SIZE) 
        {
            printk("Malformed s_uartDev message: too large\n");
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
    if(err)
    {
        return SDK_FAILURE;
    }

    return SDK_SUCCESS;
}

static void modemDeviceStart()
{
    int result = 0;
    const struct device* gpioDev = device_get_binding("GPIO_0");

    result = gpio_pin_configure(gpioDev, PIN_CATM_EN, GPIO_OUTPUT);
    __ASSERT((result >= 0),"Received error from gpio_pin_configure");
    result = gpio_pin_configure(gpioDev, PIN_PWRKET_MCU, GPIO_OUTPUT);
    __ASSERT((result >= 0),"Received error from gpio_pin_configure");

    result = gpio_pin_set(gpioDev, PIN_CATM_EN,0);
    __ASSERT((result >= 0),"Received error from gpio_pin_set");
    k_msleep(1000);
    result = gpio_pin_set(gpioDev, PIN_CATM_EN,1);
    __ASSERT((result >= 0),"Received error from gpio_pin_set");

    result = gpio_pin_set(gpioDev, PIN_PWRKET_MCU,1);
    __ASSERT((result >= 0),"Received error from gpio_pin_set");
    k_msleep(1000);
    result = gpio_pin_set(gpioDev, PIN_PWRKET_MCU,0);
    __ASSERT((result >= 0),"Received error from gpio_pin_set");
}

SDK_STAT ModemInit(ReadModemDataCB cb) 
{
    modemDeviceStart();

    s_uartDev = device_get_binding("MODEM_UART");
    if(!s_uartDev || !cb)
    {
        return SDK_FAILURE;
    }

    s_modemReadFunc = cb;
    k_msgq_init(&s_uartRxMsgq, s_uartRxMsgqBuf, 1, UART_RX_MSGQ_SIZE);
    
    k_thread_create(&s_uartReadThread, s_uartReadThreadStack, K_KERNEL_STACK_SIZEOF(s_uartReadThreadStack),
                    uartRead, NULL, NULL, NULL, K_LOWEST_APPLICATION_THREAD_PRIO, 0, K_NO_WAIT);

    uart_irq_callback_user_data_set(s_uartDev, modemUartIsr, NULL);
    uart_irq_rx_enable(s_uartDev);

    return SDK_SUCCESS;
}