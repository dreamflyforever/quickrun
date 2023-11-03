#include "static_q.h"

int queue_init(queue_t * queue,
		uint8_t* data,
		uint32_t size)
{
	int retval = -1;
	if ((data == NULL) || (size < 2) || (queue == NULL)) 
		goto out;
	
	queue->data = data;
	queue->len = 0;
	queue->size = size;
	queue->head = 0;
	queue->tail = 0;
	retval = 0;
out:
	return retval;
}

int queue_erase(queue_t * queue)
{
	int retval = -1;
	if (queue == NULL)
		goto out;

	__disable_irq();
	queue->len = 0;
	queue->head = 0;
	queue->tail = 0;
	__enable_irq();
	retval = 0;
out:
	return retval;
}

int queue_len(queue_t * queue)
{
	int retval = -1;
	if (queue == NULL)
		goto out;
	retval = queue->len;
out:
	return retval;
}

int queue_in(queue_t * queue, uint8_t * data, uint32_t len)
{
	int ret = 0;

	__disable_irq();

	if( (queue == NULL) || (data == NULL)) {
		ret = -1;
		goto out;
	}
	if (queue->len >= queue->size) {
		ret=-2;
		goto out;
	}
	/*no space to store data*/
	if (len>(queue->size - queue->len)) {
		ret=-3;
		goto out;
	}

	for (uint32_t i = 0; i < len; i++) {
		if (queue->len >= queue->size) {
			ret=-4;
			goto out;
		}
		queue->data[queue->tail] = *data++;
		queue->tail = (++queue->tail) % (queue->size);
		queue->len++;
	}

out:
	__enable_irq();
	return ret;
}

/**
 */
int queue_out(queue_t * queue, uint8_t * data, uint32_t len)
{
	int ret=0;

	__disable_irq();

	if ((queue == NULL) || (data == NULL)) {
		ret=-1;
		goto out;
	}
	/*no data*/
	if (queue->len == 0) {
		ret=0;
		goto out;
	}
	if (queue->len < len)
		len=queue->len;

	for (uint32_t i = 0; i < len; i++) {
		*data = queue->data[queue->head];
		data++;
		ret++;
		queue->head = (++queue->head) % queue->size;
		queue->len--;
	}

out:
	__enable_irq();
	return ret;
}

/**
 */
int queue_preview(queue_t * queue,
		uint8_t * data,
		uint32_t len,
		uint32_t offset)
{
	int ret=0;
	uint32_t queue_head=0;

	__disable_irq();
	if ((queue==NULL) || (data==NULL)) {
		ret=-1;
		goto out;
	}
	if (queue->len == 0) {
		ret=-2;
		goto out;
	}
	if (queue->len < (offset+len)) {
		ret=-3;
		goto out;
	}

	queue_head = (queue->head+offset) % queue->size;
	for (uint32_t i = 0; i < len; i++) {
		*data = queue->data[queue_head];
		data++;
		ret++;
		queue_head++;
		queue_head %= queue->size;
	}

out:
	__enable_irq();
	return ret;
}

int queue_delete_data(queue_t *queue,
		uint32_t len)
{
	int ret=0;

	__disable_irq();
	if (queue == NULL) {
		ret = -1;
		goto out;
	}
	if (queue->len < len)
		len = queue->len;
	queue->head = (queue->head+len)%queue->size;
	queue->len -= len;

	ret = len;
out:
	__enable_irq();
	return ret;
}

#if 0
char q[1024];

int main()
{
	char buf[10] = {0};
	queue_t entity;
	queue_init(&entity, q, 1024);
	queue_in(&entity, "abc", 3);
	queue_in(&entity, "efgh", 4);
	queue_in(&entity, "123", 3);
	queue_out(&entity, buf, 3);
	printf("%s\n", buf);
	queue_out(&entity, buf, 3);
	printf("%s\n", buf);
	queue_out(&entity, buf, 4);
	printf("%s\n", buf);
	return 0;
}
#endif
