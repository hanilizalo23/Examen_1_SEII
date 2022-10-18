#include "freertos_uart.h"

#include "fsl_uart.h"
#include "fsl_clock.h"
#include "fsl_port.h"

#include "FreeRTOS.h"
#include "semphr.h"

#if DEBUG
#include "queue.h"

#define UART_NAME(num, type, dir) ("UART"#num"."#dir"."#type)
#endif

#define NUMBER_OF_SERIAL_PORTS (2)

typedef struct
{
    uint8_t is_init;
    uart_handle_t fsl_uart_handle;
    SemaphoreHandle_t mutex_rx;
    SemaphoreHandle_t mutex_tx;
    SemaphoreHandle_t rx_sem;
    SemaphoreHandle_t tx_sem;
} freertos_uart_hanlde_t;

static freertos_uart_hanlde_t freertos_uart_handles[NUMBER_OF_SERIAL_PORTS] =
{ 0 };

static inline void freertos_uart_enable_port_clock(freertos_uart_port_t port);

static inline PORT_Type* freertos_uart_get_port_base(freertos_uart_port_t port);

static inline UART_Type* freertos_uart_get_uart_base(
        freertos_uart_number_t uart_number);

static void fsl_uart_callback(UART_Type *base, uart_handle_t *handle,
        status_t status, void *userData);

freertos_uart_flag_t freertos_uart_init(freertos_uart_config_t config)
{
    freertos_uart_flag_t retval = freertos_uart_fail;
    uart_config_t fsl_uart_config;

    if (config.uart_number < NUMBER_OF_SERIAL_PORTS)
    {
        if (!freertos_uart_handles[config.uart_number].is_init)
        {
            freertos_uart_handles[config.uart_number].mutex_rx =
                    xSemaphoreCreateMutex();
            freertos_uart_handles[config.uart_number].mutex_tx =
                    xSemaphoreCreateMutex();

            freertos_uart_handles[config.uart_number].rx_sem =
                    xSemaphoreCreateBinary();
            freertos_uart_handles[config.uart_number].tx_sem =
                    xSemaphoreCreateBinary();

            /* Clock Enable */
            freertos_uart_enable_port_clock(config.port);

            /* Port Config */
            PORT_SetPinMux(freertos_uart_get_port_base(config.port),
                    config.rx_pin, config.pin_mux);
            PORT_SetPinMux(freertos_uart_get_port_base(config.port),
                    config.tx_pin, config.pin_mux);

            UART_GetDefaultConfig(&fsl_uart_config);
            fsl_uart_config.baudRate_Bps = config.baudrate;
            fsl_uart_config.enableTx = true;
            fsl_uart_config.enableRx = true;

            if (freertos_uart0 == config.uart_number)
            {
                UART_Init(freertos_uart_get_uart_base(freertos_uart0),
                        &fsl_uart_config, CLOCK_GetFreq(UART0_CLK_SRC));
                NVIC_SetPriority(UART0_RX_TX_IRQn, 5);
            }
            else
            {
                UART_Init(freertos_uart_get_uart_base(freertos_uart1),
                        &fsl_uart_config, CLOCK_GetFreq(UART1_CLK_SRC));
                NVIC_SetPriority(UART1_RX_TX_IRQn, 5);
            }

            UART_TransferCreateHandle(
                    freertos_uart_get_uart_base(config.uart_number),
                    &freertos_uart_handles[config.uart_number].fsl_uart_handle,
                    fsl_uart_callback, NULL);

            freertos_uart_handles[config.uart_number].is_init = 1;

            retval = freertos_uart_sucess;
        }

#if DEBUG
        if (config.uart_number)
        {
            vQueueAddToRegistry(freertos_uart_handles[1].tx_sem,
                    UART_NAME(1, TX, sem));
            vQueueAddToRegistry(freertos_uart_handles[1].rx_sem,
                    UART_NAME(1, RX, sem));

            vQueueAddToRegistry(freertos_uart_handles[1].mutex_tx,
                    UART_NAME(1, TX, mut));
            vQueueAddToRegistry(freertos_uart_handles[1].mutex_rx,
                    UART_NAME(1, RX, mut));
        }
        else
        {
            vQueueAddToRegistry(freertos_uart_handles[0].tx_sem,
                    UART_NAME(0, TX, sem));
            vQueueAddToRegistry(freertos_uart_handles[0].rx_sem,
                    UART_NAME(0, RX, sem));
            vQueueAddToRegistry(freertos_uart_handles[0].mutex_tx,
                    UART_NAME(0, TX, mut));
            vQueueAddToRegistry(freertos_uart_handles[0].mutex_rx,
                    UART_NAME(0, RX, mut));
        }
#endif
    }

    return retval;
}

freertos_uart_flag_t freertos_uart_send(freertos_uart_number_t uart_number,
        uint8_t *buffer, uint16_t lenght)
{
    freertos_uart_flag_t flag = freertos_uart_fail;
    uart_transfer_t xfer;

    if (freertos_uart_handles[uart_number].is_init)
    {
        xfer.data = buffer;
        xfer.dataSize = lenght;

        xSemaphoreTake(freertos_uart_handles[uart_number].mutex_tx,
                portMAX_DELAY);

        UART_TransferSendNonBlocking(freertos_uart_get_uart_base(uart_number),
                &freertos_uart_handles[uart_number].fsl_uart_handle, &xfer);

        xSemaphoreTake(freertos_uart_handles[uart_number].tx_sem,
                portMAX_DELAY);

        xSemaphoreGive(freertos_uart_handles[uart_number].mutex_tx);

        flag = freertos_uart_sucess;
    }

    return flag;
}

freertos_uart_flag_t freertos_uart_receive(freertos_uart_number_t uart_number,
        uint8_t *buffer, uint16_t lenght)
{
    freertos_uart_flag_t flag = freertos_uart_fail;
    uart_transfer_t xfer;

    if (freertos_uart_handles[uart_number].is_init)
    {
        xfer.data = buffer;
        xfer.dataSize = lenght;

        xSemaphoreTake(freertos_uart_handles[uart_number].mutex_rx,
                portMAX_DELAY);

        UART_TransferReceiveNonBlocking(
                freertos_uart_get_uart_base(uart_number),
                &freertos_uart_handles[uart_number].fsl_uart_handle, &xfer,
                NULL);

        xSemaphoreTake(freertos_uart_handles[uart_number].rx_sem,
                portMAX_DELAY);

        xSemaphoreGive(freertos_uart_handles[uart_number].mutex_rx);

        flag = freertos_uart_sucess;
    }

    return flag;
}

static inline void freertos_uart_enable_port_clock(freertos_uart_port_t port)
{
    switch (port)
    {
    case freertos_uart_portA:
        CLOCK_EnableClock(kCLOCK_PortA);
        break;
    case freertos_uart_portB:
        CLOCK_EnableClock(kCLOCK_PortB);
        break;
    case freertos_uart_portC:
        CLOCK_EnableClock(kCLOCK_PortC);
        break;
    case freertos_uart_portD:
        CLOCK_EnableClock(kCLOCK_PortD);
        break;
    case freertos_uart_portE:
        CLOCK_EnableClock(kCLOCK_PortE);
        break;
    }
}

static inline PORT_Type* freertos_uart_get_port_base(freertos_uart_port_t port)
{
    PORT_Type *port_base = PORTA;

    switch (port)
    {
    case freertos_uart_portA:
        port_base = PORTA;
        break;
    case freertos_uart_portB:
        port_base = PORTB;
        break;
    case freertos_uart_portC:
        port_base = PORTC;
        break;
    case freertos_uart_portD:
        port_base = PORTD;
        break;
    case freertos_uart_portE:
        port_base = PORTE;
        break;
    }

    return port_base;
}

static inline UART_Type* freertos_uart_get_uart_base(
        freertos_uart_number_t uart_number)
{
    UART_Type *retval = UART0;

    switch (uart_number)
    {
    case freertos_uart0:
        retval = UART0;
        break;
    case freertos_uart1:
        retval = UART1;
        break;
    }

    return retval;
}

static void fsl_uart_callback(UART_Type *base, uart_handle_t *handle,
        status_t status, void *userData)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (kStatus_UART_TxIdle == status)
    {
        if (UART0 == base)
        {
            xSemaphoreGiveFromISR(freertos_uart_handles[freertos_uart0].tx_sem,
                    &xHigherPriorityTaskWoken);
        }
        else
        {
            xSemaphoreGiveFromISR(freertos_uart_handles[freertos_uart1].tx_sem,
                    &xHigherPriorityTaskWoken);
        }
    }

    if (kStatus_UART_RxIdle == status)
    {
        if (UART0 == base)
        {
            xSemaphoreGiveFromISR(freertos_uart_handles[freertos_uart0].rx_sem,
                    &xHigherPriorityTaskWoken);
        }
        else
        {
            xSemaphoreGiveFromISR(freertos_uart_handles[freertos_uart1].rx_sem,
                    &xHigherPriorityTaskWoken);
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
