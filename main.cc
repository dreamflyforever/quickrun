#include "core.h"

double __get_us(struct timeval t) { return (t.tv_sec * 1000000 + t.tv_usec); }

int main(int argc, char **argv)
{
	os_printf("compile time %s\n", __TIME__);
	char *model_name = NULL;
	struct timeval start_time, stop_time;
	int ret;
	session_str * entity;
	if (argc != 3) {
		printf("Usage: %s <rknn model> <jpg> \n", argv[0]);
		return -1;
	}

	model_name = (char *)argv[1];
	char * image_name = argv[2];
	
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

	return 0;
}
