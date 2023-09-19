> # quickrun

## rknn model export

参考: https://is5gnzipbc.feishu.cn/wiki/X4eGwni4YiyuUUkIrEUcYkxunHe

## quick start

**compile**

```bash
bash build_rk3588_yolov5.sh
```

**test**

```bash
bash test_rk3588_yolov5.sh
```

## `quickrun` deploy 

modify need by your project

```cpp
#define OBJ_NAME_MAX_SIZE 16  
#define OBJ_NUMB_MAX_SIZE 64  
#define OBJ_CLASS_NUM 1      
#define NMS_THRESH 0.25       
#define BOX_THRESH 0.5 
```

### license
copyright BY haoqixin.INC  
author by haoqixin AI team
