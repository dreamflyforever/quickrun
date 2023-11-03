#ifndef _STATIC_Q_
#define  _STATIC_Q_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#define __disable_irq()  
#define __enable_irq()

typedef struct queue_str {
	uint8_t * data;
	uint32_t size; /*queue size*/
	uint32_t head;
	uint32_t tail;
	uint32_t len;  /*valid lenth*/
} queue_t;

int queue_init(queue_t * queue,
		uint8_t* data,
		uint32_t size);

int queue_in(queue_t * queue, uint8_t * data, uint32_t len);
int queue_out(queue_t * queue, uint8_t * data, uint32_t len);
int queue_delete_data(queue_t *queue,
		uint32_t len);
#endif
