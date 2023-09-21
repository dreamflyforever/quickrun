/*
	This file is example for quickrun API & multi model usage.
*/

#include "core.h"
#include <pthread.h>
#include <unistd.h>

double __get_us(struct timeval t) { return (t.tv_sec * 1000000 + t.tv_usec); }

char * g_img;
void set_img(char * img)
{
	g_img = img;
}

char * get_img()
{
	return g_img;
}

typedef struct task_model_str {
	session_str * context;
	const char * model;
	char * image;
} task_model_str;

void * task_handle(void * arg)
{
	os_printf("one model\n");
	task_model_str * entity = (task_model_str * ) arg;
	entity->image = get_img();
	session_init(&(entity->context), entity->model);
	preprocess(entity->context, entity->image);
	while (1) {
		inference(entity->context);
		postprocess(entity->context);
	}
	session_deinit(entity->context);
}

typedef void * (*func_str) (void *p_arg);
int task_model_create(task_model_str * entity,
			const char * model,
			func_str func)
{
	int retval = -1;
	pthread_t task;
	entity->model = model;
	retval = pthread_create(&task, NULL, func, entity);
	if (pthread_join(task, NULL) != 0) {
			fprintf(stderr, "Failed to join thread.\n");
	}
	os_printf("create phtread success\n");
	return retval;
}

int main(int argc, char **argv)
{
	char *model_name = NULL;
	struct timeval start_time, stop_time;
	int ret;
	
	os_printf("compile time %s\n", __TIME__);
	session_str * entity;
	if (argc != 3) {
		printf("Usage: %s <rknn model> <jpg> \n", argv[0]);
		return -1;
	}

	model_name = (char *)argv[1];
	char * image_name = argv[2];
//#if test_multi_pthread_model
#if 1 
	task_model_str one_entity;
	set_img(image_name);
	task_model_create(&one_entity, model_name, task_handle);
#endif
	session_init(&entity, model_name);
	os_printf("Read %s ...\n", image_name);
	
	preprocess(entity, image_name);
	/* create the neural network */
	os_printf("Loading mode...\n");
	gettimeofday(&start_time, NULL);
	
	inference(entity);
	gettimeofday(&stop_time, NULL);
	printf("once run use %f ms\n", (__get_us(stop_time) - __get_us(start_time)) / 1000);

	postprocess(entity);

	session_deinit(entity);
	while (1) {
		sleep(10);
	}
	return 0;
}
