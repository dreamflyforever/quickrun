/*
	This file is example for quickrun API & multi model usage.
*/

#include "core.h"
#include <pthread.h>
#include <unistd.h>

#define CAPTURE_PICTURE 1

#if CAPTURE_PICTURE
#include "libv4l2.h"
#endif

#include "static_q.h"

pthread_mutex_t g_mtx;

double __get_us(struct timeval t) { return (t.tv_sec * 1000000 + t.tv_usec); }

#define test_multi_pthread_model 0
#if test_multi_pthread_model
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
	int pid;
} task_model_str;

void * task_handle(void * arg)
{
	task_model_str * entity = (task_model_str * ) arg;
	struct timeval start_time, stop_time;
	sleep(1);
	entity->image = get_img();
	session_init(&(entity->context), entity->model);
	preprocess(entity->context, entity->image);
	while (1) {
		gettimeofday(&start_time, NULL);
		inference(entity->context);
		gettimeofday(&stop_time, NULL);
		os_printf("pid: %d infernce run use %f ms\n", entity->pid, (__get_us(stop_time) - __get_us(start_time)) / 1000);
		sleep(0.1);
	}
	postprocess(entity->context);
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
	//if (pthread_join(task, NULL) != 0) {
	//		fprintf(stderr, "Failed to join thread.\n");
	//}
	os_printf("create phtread success\n");
	return retval;
}

#endif

int bbox_cb(void * arg)
{
	int retval = 0;
	os_printf("user data\n");
	return retval;
}

int framecount = 0;
time_t lasttime;
void updatefps()
{
	framecount++;

	time_t currentTime;
	time(&currentTime);

	double deltaTime = difftime(currentTime, lasttime);

	if (deltaTime >= 1) {
		double fps = framecount / deltaTime;

		os_printf(">>>>>>>> FPS: %.2f\n", fps);

		framecount = 0;
		time(&lasttime);
	}
}

#define Q_SIZE 1024
uint8 g_q[Q_SIZE];
queue_t q_entity;
void * camera_phread(void * arg)
{
	int ret;
	pthread_mutex_init(&g_mtx, NULL);
	os_printf("start camera.....\n");
	queue_init(&q_entity, g_q, Q_SIZE);
	v4l2_init();
	img_str * img;
	while (1) {
		img = capture();
		pthread_mutex_lock(&g_mtx);
		ret = queue_in(&q_entity, (uint8 *)img, sizeof(img_str));
		if (ret < 0) {
			/*TODO:*/
			os_printf("queue is overflow, maybe erase queue\n");
		}
		pthread_mutex_unlock(&g_mtx);
		os_printf("queue in ret: %d, ptr: %p\n", ret, img->ptr);
		usleep(25000);
		free(img);
	}
}

int start_camera()
{
	int retval;
	pthread_t camera_task;
	retval = pthread_create(&camera_task, NULL, camera_phread, NULL);
	return retval;
}

/*port camera API*/
img_str * quickrun_capture()
{
	img_str * img  = (img_str *)malloc(sizeof(img_str));
	int ret;
	pthread_mutex_lock(&g_mtx);
	ret = queue_out(&q_entity, (uint8 *)img, sizeof(img_str));
	pthread_mutex_unlock(&g_mtx);
	if (ret == 0 || ret == -1) {
		free(img);
		img = NULL;
	} else {
		os_printf("queue out ret: %d, ptr: %p\n", ret, img->ptr);
	}
	return img;
}

int main(int argc, char **argv)
{
	char *model_name = NULL;
	struct timeval start_time, stop_time;
	int ret;
#if CAPTURE_PICTURE
	img_str * get_picture = NULL;
#endif
	os_printf("compile time %s\n", __TIME__);
	session_str * entity;
	if (argc != 3) {
		printf("Usage: %s <rknn model> <jpg> \n", argv[0]);
		//return -1;
	}

	//model_name = (char *)argv[1];
	//char * image_name = argv[2];

	//model_name = "assets/yolov5s_logo_quan_0918.rknn";
	model_name = "assets/yolov5s_bottle_quan_1102.rknn";
	char * image_name = "assets/logo.jpg";

#define test_multi_pthread_model 0
#if test_multi_pthread_model
	/*one model*/
	task_model_str one_entity;
	one_entity.pid = 1;
	set_img(image_name);
	task_model_create(&one_entity, model_name, task_handle);
	/*send model*/
	task_model_str two_entity;
	two_entity.pid = 2;
	set_img(image_name);
	task_model_create(&two_entity, model_name, task_handle);

	/*third model*/
	task_model_str three_entity;
	three_entity.pid = 3;
	set_img(image_name);
	task_model_create(&three_entity, model_name, task_handle);
#endif
	start_camera();
	session_init(&entity, model_name);
	set_user_cb(entity, bbox_cb);

	os_printf("Read %s ...\n", image_name);

	while (1) {
#if CAPTURE_PICTURE
		get_picture = quickrun_capture();
		if (get_picture == NULL) {
			os_printf("picture NULL from the queue\n");
			usleep(25000);
			continue;
		}
		//usleep(25000);
		preprocess(entity, get_picture);
#else
		preprocess(entity, image_name);
#endif
		/* create the neural network */
		os_printf("inference ...\n");
		gettimeofday(&start_time, NULL);

		inference(entity);
		gettimeofday(&stop_time, NULL);
		printf("once run use %f ms\n", (__get_us(stop_time) - __get_us(start_time)) / 1000);

		postprocess(entity);
		updatefps();
		os_printf("main runing\n");
#if CAPTURE_PICTURE
		free(get_picture->ptr);
		free(get_picture);
#endif
	}
	session_deinit(entity);
	return ret;
}
