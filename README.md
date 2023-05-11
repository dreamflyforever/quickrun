> # 新的 Readme

## 一、rknn 模型的导出

请参考其他博客和仓库；

[CSDN 黄金旺铺](https://blog.csdn.net/zhoujinwang/article/details/130524310?spm=1001.2014.3001.5501)

[github仓库](https://github.com/wangqiqi/yolov5/tree/rknn_dev)

## 二、测试验证

1. **`git clone` 仓库到本地**

**NOTE: 此处说的本地就是开发板，需要将开发板联网，并配置相关git信息**

2. **编译安装**

```bash
bash build_rk3588_yolov5.sh
```

3. **测试**

```bash
bash test_rk3588_yolov5.sh
```

## 三、`rknn` 部署自己的模型和项目

1. 将导出的 `rknn` 模型，放到 `assets` 文件夹下；
2. 修改 `assets/labels_list.txt` 文件，将训练的目标类别名称分行存储；
3. 添加测试图片到 `assets` 文件夹下；
4. 根据项目需要，修改文件中相关信息

文件 `yolov5/include/postprocess.h` 中 `line 7~11`

```cpp
#define OBJ_NAME_MAX_SIZE 16  // 最长目标名称
#define OBJ_NUMB_MAX_SIZE 64  // 最多目标个数
#define OBJ_CLASS_NUM 1       // 目标类别数--需要根据项目进行修改
#define NMS_THRESH 0.25       // NMS 阈值
#define BOX_THRESH 0.5       // 目标置信度
```

5. 编译安装与测试

```bash
bash build_rk3588_yolov5.sh
```

> 修改脚本`test_rk3588_yolov5.sh` 中不同的模型和测试图片

```bash
set -e

ROOT_PWD=$( cd "$( dirname $0 )" && cd -P "$( dirname "$SOURCE" )" && pwd )
INSTALL_DIR=${ROOT_PWD}/install

cd ${INSTALL_DIR}
./rk3588_yolov5 assets/drp.rknn assets/drp.png
cd -
```

然后执行

```bash
bash test_rk3588_yolov5.sh
```

## 四、清理

```bash
bash clean_all.sh
```


> **NOTE: 以下为原仓库中的readme**
> # Yolo-v5 demo
>
> ## 导出rknn模型
>
> 请参考 https://github.com/airockchip/rknn_model_zoo/tree/main/models/vision/object_detection/yolov5-pytorch
>
> ## 注意事项
>
> 1. 使用rknn-toolkit2版本大于等于1.1.2。
> 2. 切换成自己训练的模型时，请注意对齐anchor等后处理参数，否则会导致后处理解析出错。
> 3. 官网和rk预训练模型都是检测80类的目标，如果自己训练的模型,需要更改include/postprocess.h中的OBJ_CLASS_NUM以及NMS_THRESH,BOX_THRESH后处理参数。
> 4. demo需要librga.so的支持,编译使用请参考https://github.com/rockchip-linux/linux-rga
> 5. 由于硬件限制，该demo的模型默认把 yolov5 模型的后处理部分，移至cpu实现。本demo附带的模型均使用relu为激活函数，相比silu激活函数精度略微下降，性能大幅上升。
>
> ## Android Demo
>
> ### 编译
>
> 根据指定平台修改 `build-android_<TARGET_PLATFORM>.sh`中的Android NDK的路径 `ANDROID_NDK_PATH`，<TARGET_PLATFORM>可以是RK356X或RK3588 例如修改成：
> ```sh
> ANDROID_NDK_PATH=~/opt/tool_chain/android-ndk-r17
> ```
> 然后执行：
> 
> ```sh
> ./build-android_<TARGET_PLATFORM>.sh
> ```
> 
> ### 推送执行文件到板子
> 
> 连接板子的usb口到PC,将整个demo目录到 `/data`:
> 
> ```sh
> adb root
> adb remount
> adb push install/rknn_yolov5_demo /data/
> ```
> 
> ### 运行
> 
> ```sh
> adb shell
> cd /data/rknn_yolov5_demo/
> export LD_LIBRARY_PATH=./lib
> ./rknn_yolov5_demo model/<TARGET_PLATFORM>/yolov5s-640-640.rknn model/bus.jpg
> ```
> ## Aarch64 Linux Demo
> 
> ### 编译
>
> 根据指定平台修改 `build-linux_<TARGET_PLATFORM>.sh`中的交叉编译器所在目录的路径 `TOOL_CHAIN`，例如修改成
>
> ```sh
> export TOOL_CHAIN=~/opt/tool_chain/gcc-9.3.0-x86_64_aarch64-linux-gnu/host
> ```
> 
> 然后执行：
> 
> ```sh
> ./build-linux_<TARGET_PLATFORM>.sh
> ```
> 
> ### 推送执行文件到板子
>
> 将 install/rknn_yolov5_demo_Linux 拷贝到板子的/userdata/目录.
>
> 如果使用rockchip的EVB板子，可以使用adb将文件推到板子上：
>
> ```
> adb push install/rknn_yolov5_demo_Linux /userdata/
> ```
> 如果使用其他板子，可以使用scp等方式将install/rknn_yolov5_demo_Linux拷贝到板子的/userdata/目录
>
> ### 运行
> 
> ```sh
> adb shell
> cd /userdata/rknn_yolov5_demo_Linux/
>
> export LD_LIBRARY_PATH=./lib
> ./rknn_yolov5_demo model/<TARGET_PLATFORM>/yolov5s-640-640.rknn model/bus.jpg
> ```
> Note: Try searching the location of librga.so and add it to LD_LIBRARY_PATH if the librga.so is not found on the lib folder.
> Using the following commands to add to LD_LIBRARY_PATH.
> 
> ```sh
> export LD_LIBRARY_PATH=./lib:<LOCATION_LIBRGA.SO>
> ```
> ### 注意
> 需要根据系统的rga驱动选择正确的librga库，具体依赖请参考： https://github.com/airockchip/librga
