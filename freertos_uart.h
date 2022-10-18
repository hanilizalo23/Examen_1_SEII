
#ifndef RTOS_UART_H_
#define RTOS_UART_H_

#include "stdint.h"

typedef enum
{
  freertos_uart0,
  freertos_uart1
} freertos_uart_number_t;

typedef enum
{
  freertos_uart_portA,
  freertos_uart_portB,
  freertos_uart_portC,
  freertos_uart_portD,
  freertos_uart_portE
} freertos_uart_port_t;

typedef enum
{
  freertos_uart_sucess,
  freertos_uart_fail
} freertos_uart_flag_t;

typedef struct
{
	uint32_t baudrate;
	freertos_uart_number_t uart_number;
	freertos_uart_port_t port;
	uint8_t rx_pin;
	uint8_t tx_pin;
	uint8_t pin_mux;
} freertos_uart_config_t;

freertos_uart_flag_t freertos_uart_init(freertos_uart_config_t config);

freertos_uart_flag_t freertos_uart_send(freertos_uart_number_t uart_number,uint8_t * buffer, uint16_t lenght);

freertos_uart_flag_t freertos_uart_receive(freertos_uart_number_t uart_number, uint8_t * buffer, uint16_t lenght);

#endif /* RTOS_UART_H_ */
