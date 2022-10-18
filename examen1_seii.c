/**
 * @file    examen1_seii.c
 * @brief   Application entry point.
 */

#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MK66F18.h"
#include "fsl_debug_console.h"

/* TODO: insert other include files here. */
#include "semphr.h"
#include "task.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "event_groups.h"
#include "freertos_uart.h"

/* TODO: insert other definitions and declarations here. */
void initial_task(void *pvParameters);
void first_task(void *pvParameters);
void second_task(void *pvParameters);
void third_task(void *pvParameters);

#define NUM_TASKS 	4
#define FIRST_TASK_P 1
#define FIRST_TASK_EVENT  1<<0
#define SECOND_TASK_EVENT 1<<1
#define SENDER_ONE	1
#define SENDER_TWO 	2

/*Semaphores mutex for give and take the buffer space message*/
SemaphoreHandle_t send_msg_mutex;
SemaphoreHandle_t take_msg_mutex;
QueueHandle_t msg_buffer;
EventGroupHandle_t msg_event;
typedef uint8_t sender;
sender sender_number;

typedef struct {
	const uint8_t *msg;
} message_t;

int main(void) {

  	/* Init board hardware. */
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
#ifndef BOARD_INIT_DEBUG_CONSOLE_PERIPHERAL
    /* Init FSL debug console. */
    BOARD_InitDebugConsole();
#endif

    PRINTF("Examen 1\n\r ");

    while(!xTaskCreate(initial_task, "initial_task", configMINIMAL_STACK_SIZE+100, NULL, FIRST_TASK_P, NULL))
    {
    	PRINTF("Failed to create Task \n\r");
    }

    vTaskStartScheduler();

    while(1)
    {
    	;
    }

    return 0;
}

void initial_task(void *pvParameters)
{
	//Tasks creation
	while(!xTaskCreate(first_task, "send_msg1", configMINIMAL_STACK_SIZE+100, NULL, NUM_TASKS -1, NULL))
	{
		PRINTF("Failed to create Task 1 \n\r");
	}

	while(!xTaskCreate(second_task, "send_msg2", configMINIMAL_STACK_SIZE+100, NULL, NUM_TASKS -1 , NULL))
	{
		PRINTF("Failed to create Task 2\n\r");
	}

	while(!xTaskCreate(third_task, "receive_msg", configMINIMAL_STACK_SIZE+100, NULL, NUM_TASKS -1, NULL))
	{
		PRINTF("Failed to create Task 3\n\r");
	}

	//UART configuration
    freertos_uart_config_t config;

    config.baudrate = 9600;
    config.rx_pin = 16;
    config.tx_pin = 17;
    config.pin_mux = kPORT_MuxAlt3;
    config.uart_number = freertos_uart0;
    config.port = freertos_uart_portB;
    freertos_uart_init(config);

	msg_buffer = xQueueCreate(2, sizeof(message_t));
	msg_event  = xEventGroupCreate();
	send_msg_mutex = xSemaphoreCreateMutex();
	take_msg_mutex = xSemaphoreCreateMutex();

	vTaskSuspend(NULL);
}

void first_task(void *pvParameters)
{
	//Send message
	const uint8_t msg_1[] = "First task\n\r";
	message_t buffer_msg;
	buffer_msg.msg = msg_1;

	//Block status
	const TickType_t delay = pdMS_TO_TICKS(150U);
	sender_number = SENDER_ONE;
	for(;;)
	{
		//Take the mutex to send the message
		xSemaphoreTake(send_msg_mutex, portMAX_DELAY);

		//Send message
		freertos_uart_send(freertos_uart0, &buffer_msg.msg, 10);
		xQueueSend(msg_buffer, &buffer_msg, portMAX_DELAY);

		//Free the mutex to be used by other task
		xSemaphoreGive(send_msg_mutex);

		//Set the event bit event for this task
		xEventGroupSetBits(msg_event, FIRST_TASK_EVENT);

		//Blocking
		vTaskDelay(delay);
	}
}

void second_task(void *pvParameters)
{
	//Send message
	const uint8_t msg_2[] = "Second task\n\r";
	message_t buffer_msg;
	buffer_msg.msg = msg_2;

	//Block status
	const TickType_t delay = pdMS_TO_TICKS(200U);
	sender_number = SENDER_ONE;

	for(;;)
	{
		//Take the mutex to send the message
		xSemaphoreTake(send_msg_mutex, portMAX_DELAY);

		//Send message
		freertos_uart_send(freertos_uart0, &buffer_msg.msg, 10);
		xQueueSend(msg_buffer, &buffer_msg, portMAX_DELAY);

		//Free the mutex to be used by other task
		xSemaphoreGive(send_msg_mutex);

		//Set the event bit event for this task
		xEventGroupSetBits(msg_event, SECOND_TASK_EVENT);

		//Blocking
		vTaskDelay(delay);
	}
}

void third_task(void *pvParameters)
{
	message_t buffer_msg;

	//Receive message
	for(;;)
	{
		//Wait for message
		xEventGroupWaitBits(msg_event, (SECOND_TASK_EVENT | FIRST_TASK_EVENT), NULL, NULL, portMAX_DELAY);
		xQueueReceive(msg_buffer, &buffer_msg, portMAX_DELAY);

		//Send and take message are bussy
		xSemaphoreTake(send_msg_mutex,portMAX_DELAY);
		xSemaphoreTake(take_msg_mutex,portMAX_DELAY);

		//The message is completed so lets print it
		freertos_uart_receive(freertos_uart0, &buffer_msg.msg, 10);

		//Free the mutex to be used by other task
		xSemaphoreGive(take_msg_mutex);

		//Clear manually the even group bits for each task
		switch(sender_number)
		{
		case(FIRST_TASK_EVENT):
				xEventGroupClearBits(msg_event, FIRST_TASK_EVENT);
				break;
		case(SECOND_TASK_EVENT):
				xEventGroupClearBits(msg_event, SECOND_TASK_EVENT);
				break;
		}

		//Free the mutex to be used by other task
		xSemaphoreGive(send_msg_mutex);
	}
}
