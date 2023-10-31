/*
	License by Haoqixin.INC
	author: AI team
*/
#include "core.h"

long long get_timestamp(void)//获取时间戳函数
{
    long long tmp;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    tmp = tv.tv_sec;
    tmp = tmp * 1000;
    tmp = tmp + (tv.tv_usec / 1000);

    return tmp;
}

void dump_tensor_attr(rknn_tensor_attr *attr)
{
	os_printf("  index=%d, name=%s, n_dims=%d, dims=[%d, %d, %d, %d],"
		"n_elems=%d, size=%d, fmt=%s, type=%s, qnt_type=%s, "
		"zp=%d, scale=%f\n",
		attr->index, attr->name, attr->n_dims, 
		attr->dims[0], attr->dims[1], attr->dims[2], attr->dims[3],
		attr->n_elems, attr->size, get_format_string(attr->fmt),
		get_type_string(attr->type), get_qnt_type_string(attr->qnt_type),
		attr->zp, attr->scale);
}

uint8 * load_data(FILE *fp, size_t ofst, size_t sz)
{
	uint8 * data;
	int ret;

	data = NULL;

	if (NULL == fp) {
		goto end;	
	}

	ret = fseek(fp, ofst, SEEK_SET);
	if (ret != 0) {
		os_printf("blob seek failure.\n");
		goto end;
	}

	data = (uint8 *)malloc(sz);
	if (data == NULL) {
		os_printf("buffer malloc failure.\n");
		goto end;
	}
	ret = fread(data, 1, sz, fp);
end:
	return data;
}

uint8 *load_model(const char *filename, int *model_size)
{
	FILE * fp;
	uint8 * data = NULL;
	int size;
	fp = fopen(filename, "rb");
	if (NULL == fp) {
		os_printf("Open file %s failed.\n", filename);
		goto end;
	}

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);

	data = load_data(fp, 0, size);

	fclose(fp);

	*model_size = size;
end:
	return data;
}

int saveFloat(const char *file_name, float *output, int element_size)
{
	FILE *fp;
	int i;
	fp = fopen(file_name, "w");
	if (fp == NULL) {
		os_printf("file no found\n");
		goto end;
	}
	for (i = 0; i < element_size; i++) {
		fprintf(fp, "%.6f\n", output[i]);
	}
	fclose(fp);
end:
	return 0;
}

int preprocess(session_str * entity, const char * image_name)
{
	int ret = -1;
	int img_width = 0;
	int img_height = 0;
	int img_channel = 0;

	int width = entity->model_width;
	int height = entity->model_height;
	int channel = entity->model_channel;
	/*init rga context*/
	rga_buffer_t src;
	rga_buffer_t dst;
	im_rect src_rect;
	im_rect dst_rect;
	memset(&src_rect, 0, sizeof(src_rect));
	memset(&dst_rect, 0, sizeof(dst_rect));
	memset(&src, 0, sizeof(src));
	memset(&dst, 0, sizeof(dst));
	long long start = get_timestamp();
	entity->orig_img = cv::imread(image_name, 1);
	cv::Mat img;
	cv::cvtColor(entity->orig_img, img, cv::COLOR_BGR2RGB);
	long long end = get_timestamp();
	os_printf("delay: %d\n", (end - start));
	if (!entity->orig_img.data) {
		os_printf("cv::imread %s fail!\n", image_name);
		goto end;
	}
	img_width = img.cols;
	img_height = img.rows;
	os_printf("img width = %d, img height = %d\n", img_width, img_height);
	entity->img_height = img_height;
	entity->img_width = img_width;
	/* You may not need resize when src resulotion equals to dst resulotion */
	entity->resize_buf = nullptr;

	if (img_width != width || img_height != height) {
		os_printf("resize with RGA!\n");
		entity->resize_buf = malloc(height * width * channel);
		memset(entity->resize_buf, 0x00, height * width * channel);

		src = wrapbuffer_virtualaddr((void *)img.data, img_width, img_height, RK_FORMAT_RGB_888);
		dst = wrapbuffer_virtualaddr((void *)entity->resize_buf, width, height, RK_FORMAT_RGB_888);
		ret = imcheck(src, dst, src_rect, dst_rect);
		if (IM_STATUS_NOERROR != ret) {
			os_printf("%d, check error! %s", __LINE__, imStrError((IM_STATUS)ret));
			goto end;
		}
		IM_STATUS STATUS = imresize(src, dst);

		// for debug
		cv::Mat resize_img(cv::Size(width, height), CV_8UC3, entity->resize_buf);
		cv::imwrite("resize_input.jpg", resize_img);

		entity->inputs[0].buf = entity->resize_buf;
	} else {
		entity->inputs[0].buf = (void *)img.data;
	}
	rknn_inputs_set(entity->ctx, (entity->io_num).n_input, entity->inputs);

	entity->outputs = (rknn_output * )malloc(entity->io_num.n_output * sizeof(rknn_output));
	memset(entity->outputs, 0, entity->io_num.n_output * sizeof(rknn_output));
	for (int i = 0; i < entity->io_num.n_output; i++) {
		entity->outputs[i].want_float = 0;
	}
end:
	if (entity->resize_buf)
		free(entity->resize_buf);
	return ret;
}

int postprocess(session_str * entity)
{
	int retval = 0;
	int width = entity->model_width;
	int height = entity->model_height;
	int channel = entity->model_channel;
	float scale_w = (float)width / entity->img_width;
	float scale_h = (float)height / entity->img_height;

	const float nms_threshold = NMS_THRESH;
	const float box_conf_threshold = BOX_THRESH;

	os_printf("post process config: box_conf_threshold = %.2f,"
		"nms_threshold = %.2f\n", box_conf_threshold, nms_threshold);

	detect_result_group_t detect_result_group;
	std::vector<float> out_scales;
	std::vector<int32_t> out_zps;
	for (int i = 0; i < entity->io_num.n_output; ++i) {
		out_scales.push_back(entity->output_attrs[i].scale);
		out_zps.push_back(entity->output_attrs[i].zp);
	}
	rknn_output *outputs = entity->outputs;
	post_process((int8_t *)outputs[0].buf, (int8_t *)outputs[1].buf,
			(int8_t *)outputs[2].buf, height, width,
			box_conf_threshold, nms_threshold, scale_w,
			scale_h, out_zps, out_scales, &detect_result_group);
	if (entity->outputs)
		free(entity->outputs);
	/*Draw Objects*/
	char text[256];
	for (int i = 0; i < detect_result_group.count; i++) {
		detect_result_t *det_result = &(detect_result_group.results[i]);
		sprintf(text, "%s %.1f%%", det_result->name, det_result->prop * 100);
		os_printf("%s @ (%d %d %d %d) %f\n", det_result->name, det_result->box.left, det_result->box.top,
			   det_result->box.right, det_result->box.bottom, det_result->prop);
		int x1 = det_result->box.left;
		int y1 = det_result->box.top;
		int x2 = det_result->box.right;
		int y2 = det_result->box.bottom;
		rectangle(entity->orig_img, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(255, 0, 0, 255), 3);
		putText(entity->orig_img, text, cv::Point(x1, y1 + 12), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0));
	}

	imwrite("./out.jpg", entity->orig_img);
	retval = rknn_outputs_release(entity->ctx,
		entity->io_num.n_output, entity->outputs);
	return retval;
}

int session_init(session_str ** entity, const char * model_name)
{
	int ret = -1;
	int channel = 3;
	int width = 0;
	int height = 0;
	int i;
	*entity = (session_str *)malloc(sizeof(session_str));
	int model_data_size = 0;
	(*entity)->model_data = load_model(model_name, &model_data_size);
	ret = rknn_init(&((*entity)->ctx), (*entity)->model_data, model_data_size, 0, NULL);
	if (ret < 0) {
		os_printf("rknn_init error ret=%d\n", ret);
		goto end;	
	}

	rknn_sdk_version version;
	ret = rknn_query((*entity)->ctx, RKNN_QUERY_SDK_VERSION,
			&version, sizeof(rknn_sdk_version));
	if (ret < 0) {
		os_printf("rknn_init error ret=%d\n", ret);
		goto end;	
	}

	os_printf("sdk version: %s driver version: %s\n",
		version.api_version, version.drv_version);

	ret = rknn_query((*entity)->ctx, RKNN_QUERY_IN_OUT_NUM,
			&((*entity)->io_num), sizeof((*entity)->io_num));
	if (ret < 0) {
		os_printf("rknn_init error ret=%d\n", ret);
		goto end;
	}

	os_printf("model input num: %d, output num: %d\n",
		((*entity)->io_num).n_input, ((*entity)->io_num).n_output);

	(*entity)->input_attrs = (rknn_tensor_attr *)malloc(((*entity)->io_num).n_input * sizeof(rknn_tensor_attr));
	memset((*entity)->input_attrs, 0, ((*entity)->io_num).n_input * sizeof(rknn_tensor_attr));
	os_printf(">>>>>>>>>%d\n", sizeof((*entity)->input_attrs));
	for (i = 0; i < ((*entity)->io_num).n_input; i++) {
		((*entity)->input_attrs[i]).index = i;
		ret = rknn_query((*entity)->ctx, RKNN_QUERY_INPUT_ATTR,
				&((*entity)->input_attrs[i]), sizeof(rknn_tensor_attr));
		if (ret < 0) {
			os_printf("rknn_init error ret=%d\n", ret);
			return -1;
		}
		dump_tensor_attr(&((*entity)->input_attrs[i]));
	}
	(*entity)->output_attrs  = (rknn_tensor_attr *)malloc(((*entity)->io_num).n_output * sizeof(rknn_tensor_attr));
	memset((*entity)->output_attrs, 0, ((*entity)->io_num).n_output * sizeof(rknn_tensor_attr));
	for (i = 0; i < ((*entity)->io_num).n_output; i++) {
		((*entity)->output_attrs[i]).index = i;
		ret = rknn_query((*entity)->ctx, RKNN_QUERY_OUTPUT_ATTR,
				&(((*entity)->output_attrs)[i]), sizeof(rknn_tensor_attr));
		dump_tensor_attr(&((*entity)->output_attrs[i]));
	}
	if ((*entity)->input_attrs[0].fmt == RKNN_TENSOR_NCHW) {
		os_printf("model is NCHW input fmt\n");
		channel = (*entity)->input_attrs[0].dims[1];
		height  = (*entity)->input_attrs[0].dims[2];
		width   = (*entity)->input_attrs[0].dims[3];
	} else {
		os_printf("model is NHWC input fmt\n");
		height =  (*entity)->input_attrs[0].dims[1];
		width  =  (*entity)->input_attrs[0].dims[2];
		channel = (*entity)->input_attrs[0].dims[3];
	}

	os_printf("model input height=%d, width=%d, channel=%d\n", height, width, channel);
	(*entity)->model_height = height;
	(*entity)->model_width = width;
	(*entity)->model_channel = channel;
	memset((*entity)->inputs, 0, sizeof((*entity)->inputs));
	(*entity)->inputs[0].index = 0;
	(*entity)->inputs[0].type = RKNN_TENSOR_UINT8;
	(*entity)->inputs[0].size = width * height * channel;
	(*entity)->inputs[0].fmt = RKNN_TENSOR_NHWC;
	(*entity)->inputs[0].pass_through = 0;

end:
	return ret;
}

int session_deinit(session_str * entity)
{
	int retval = -1;
	// release
	retval = rknn_destroy(entity->ctx);

	if (entity->model_data) {
		free(entity->model_data);
	}

	if (entity->resize_buf) {
		free(entity->resize_buf);
	}
	free(entity->input_attrs);
	free(entity->output_attrs);
	if (entity->outputs)
		free(entity->outputs);
	free(entity);

	return retval;
}

int inference(session_str * entity)
{
	int retval = -1;
	if (entity == NULL) {
		os_printf("error\n");
		assert(0);
	}
	os_printf("======\n");
	if (entity->ctx == NULL)
		os_printf("======\n");
	if (entity->outputs == NULL)	
		os_printf("======\n");
	os_printf("entity: %p, ctx: %p\n", entity, entity->ctx);
	retval = rknn_run(entity->ctx, NULL);
	os_printf("======\n");
	retval = rknn_outputs_get(entity->ctx, entity->io_num.n_output,
				entity->outputs, NULL);
	os_printf("======\n");
	return retval;
}

/* test API */
#if 0
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
#endif
