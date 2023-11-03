/*
	License by Haoqixin.INC
	author: AI team
*/
#ifndef _CORE_H_
#define _CORE_H_

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define _BASETSD_H

#include "RgaUtils.h"
#include "im2d.h"
#include "opencv2/core/core.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include "postprocess.h"
#include "rga.h"
#include "rknn_api.h"

#define uint8 unsigned char 

typedef int (*USER_CB) (void * p_arg);

typedef struct session_str {
	rknn_context ctx;
	int model_width;
	int model_height;
	int model_channel = 3;
	rknn_input_output_num io_num;
	//rknn_tensor_attr input_attrs[io_num.n_input];
	rknn_tensor_attr * input_attrs;//[io_num.n_input];
	rknn_tensor_attr * output_attrs;
	void * resize_buf = nullptr;
	rknn_input inputs[1];
	rknn_output *outputs;//[io_num.n_output];
	int img_width;
	int img_height;
	int img_channel;
	uint8 * model_data;
	cv::Mat orig_img;
	USER_CB cb;
} session_str;

typedef struct {
	char *ptr;
	int size;
} img_str;

void dump_tensor_attr(rknn_tensor_attr *attr);
uint8 * load_data(FILE *fp, size_t ofst, size_t sz);
uint8 *load_model(const char *filename, int *model_size);
int saveFloat(const char *file_name, float *output, int element_size);

/* user API for AI engine */
int preprocess(session_str * entity, img_str * image_name);
int postprocess(session_str * entity);
int session_init(session_str ** entity, const char * model_name);
int session_deinit(session_str * entity);
int inference(session_str * entity);
int set_user_cb(session_str * entity, USER_CB cb);

/* debug printf*/
#define DEBUG 1
#if DEBUG
#define os_printf(format, ...) \
	{printf("[%s : %s : %d] ", \
	__FILE__, __func__, __LINE__); \
	printf(format, ##__VA_ARGS__);}
#else
#define os_printf(format, ...) 
#endif

#endif
