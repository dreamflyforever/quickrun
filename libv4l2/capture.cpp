#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <time.h>

#include "find_usbdevice.h"
#include "libv4l2.h"
//#define FORMAT V4L2_PIX_FMT_YUYV

#define FORMAT V4L2_PIX_FMT_MJPEG
//#define FORMAT V4L2_PIX_FMT_H264

int set_exposure(int Handle, int exposure)
{
	int ret;
    struct v4l2_control ctrl;

    int try_cnt = 0;

    while(try_cnt < 5)
    {
        ctrl.id = V4L2_CID_EXPOSURE_AUTO;
        ctrl.value = 0;//V4L2_EXPOSURE_AUTO;
        
        ret = ioctl(Handle, VIDIOC_G_CTRL, &ctrl);
        if (ret < 0)
        {
            printf("get exposure mode failed (%d) errno:%d", ret, errno);
            ctrl.value = V4L2_EXPOSURE_AUTO;
        }
        else
        {
            printf("get exposure mode successful, mode %d", ctrl.value);
        }

        if(ctrl.value != V4L2_EXPOSURE_MANUAL)
        {
            ctrl.id = V4L2_CID_EXPOSURE_AUTO; 
            ctrl.value = V4L2_EXPOSURE_MANUAL;
            ret = ioctl(Handle, VIDIOC_S_CTRL, &ctrl); 
            if (ret < 0) 
            {
                printf("set exposure manual failed (%d), try again %d", ret, try_cnt);
                //return false;
            }
            else
            {
                printf("set exposure manual successful");
                break;
            }
        }
        else
        {
            break;
        }

        try_cnt++;
        usleep(200000);
    }

    ctrl.id = V4L2_CID_EXPOSURE_ABSOLUTE;
    ret = ioctl(Handle, VIDIOC_G_CTRL, &ctrl); 
    if (ret < 0) 
    {
        printf("get current abs exposure failed (%d)", ret);
        return -1;
    }
    else
    {
        printf("get current abs exposure : %d", ctrl.value);
    }

    ctrl.id = V4L2_CID_EXPOSURE_ABSOLUTE;
    ctrl.value = exposure;
    ret = ioctl(Handle, VIDIOC_S_CTRL, &ctrl);
    if (ret < 0) 
    {
        printf("set exposure failed (%d)", ret); 
        return -1;
    }
    else
    {
        printf("set exposure %d successful", ctrl.value);
    }

    return 0;
}

img_str * capture()
{
	int fd;
	int ret;
	int num;
	char name[100] = {0};
	int width = 640, height = 360;
	int nr_bufs = 4;
	int i;
	struct v4l2_capability cap;
	struct v4l2_buf* v4l2_buf;
	struct v4l2_buf_unit* v4l2_buf_unit;
	FILE * fp;
	char *str = (char *)malloc(20);
	memset(str, 0, 20);
	int t;
	char dev_name[10] = {0};
	char DEFAULT_DEV[13] = {0};
	img_str * img = NULL;
	memset(&cap, 0, sizeof(cap));
	int argc = 3;
	char argv[][13] = {"", "/dev/video4", "80"};
	if (argc != 3) {
		/*check pid-vid search the device*/
		#if 1
        	get_usbdevname("0559","2bc5", video, dev_name);
        	//get_usbdevname("9230","05a3", video, dev_name);
        	//get_usbdevname("636a","0c46", video, dev_name);
        	//get_usbdevname("636b","0c45", video, dev_name);
		snprintf(DEFAULT_DEV, 13, "/dev/%s", dev_name);
		#endif
		num = 80;
		printf("Usage:%s </dev/videox> exposure, default: %s, exposure: %d\n",
			argv[0], DEFAULT_DEV, num);
		chdir("/data/");
		//return -1;
	} else {
		strncpy(DEFAULT_DEV, argv[1], 13);
		num = atoi(argv[2]);
	}
	printf("%s\n", DEFAULT_DEV);
	t = time((time_t*)NULL);
	sprintf(str, "%d.jpeg", t);
	printf("str: %s\n", str);
	fp = fopen(str, "w+");
	if (!fp) {
		perror("failed to open picture");
		return NULL;
	}

	fd = v4l2_open(DEFAULT_DEV, O_RDWR);
	if(fd < 0)
		goto err;

	ret = v4l2_querycap(fd, &cap);
	if(ret < 0)
		goto err;

	if(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)
		printf("dev support capture\n");

	if(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)
		printf("dev support output\n");

	if(cap.capabilities & V4L2_CAP_VIDEO_OVERLAY)
		printf("dev support overlay\n");

	if(cap.capabilities & V4L2_CAP_STREAMING)
		printf("dev support streaming\n");

	if(cap.capabilities & V4L2_CAP_READWRITE)
		printf("dev support read write\n");

	//set_exposure(fd, num);
	ret = v4l2_enuminput(fd, 0, name);
	if(ret < 0)
		goto err;
	printf("input device name:%s\n", name);

	ret = v4l2_s_input(fd, 0);
	if(ret < 0)
		goto err;

	ret = v4l2_enum_fmt(fd, FORMAT, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	if(ret < 0)
		goto err;

	ret = v4l2_s_fmt(fd, &width, &height, FORMAT, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	if(ret < 0)
		goto err;
	printf("image width:%d\n", width);
	printf("image height:%d\n", height);

	v4l2_buf = v4l2_reqbufs(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, nr_bufs);
	if(!v4l2_buf)
		goto err;

	ret = v4l2_querybuf(fd, v4l2_buf);
	if(ret < 0)
		goto err;

	ret = v4l2_mmap(fd, v4l2_buf);
	if(ret < 0)
		goto err;

	ret = v4l2_qbuf_all(fd, v4l2_buf);
	if(ret < 0)
		goto err;

	ret = v4l2_streamon(fd);
	if(ret < 0)
		goto err;

	ret = v4l2_poll(fd);
	if(ret < 0)
		goto err;

	v4l2_buf_unit = v4l2_dqbuf(fd, v4l2_buf);
	if(!v4l2_buf_unit)
		goto err;

	fwrite(v4l2_buf_unit->start, 1, v4l2_buf_unit->length, fp);
	img = (img_str *)malloc(sizeof(img_str));
	img->size = v4l2_buf_unit->length;
	img->ptr = (char * )malloc(img->size);
	memcpy(img->ptr, v4l2_buf_unit->start, img->size);
	ret = v4l2_qbuf(fd, v4l2_buf_unit);
	if(ret < 0)
		goto err;

	ret = v4l2_streamoff(fd);
	if(ret < 0)
		goto err;

	ret = v4l2_munmap(fd, v4l2_buf);
	if(ret < 0)
		goto err;

	ret = v4l2_relbufs(v4l2_buf);
	if(ret < 0)
		goto err;

	v4l2_close(fd);
	fclose(fp);
	free(str);
	//return str;
	return img;

err:
	perror("err");

	return NULL;
}
