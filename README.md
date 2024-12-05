> # quickrun

Quickrun is a software for the efficient and high-concurrency deployment of multiple models on the rk3588 rknn.

Software Framework:

	1. Adopts the session idea, allowing the definition of multiple sessions to meet the different task requirements of various models. For example, in the case of charging pile detection, garbage classification, and cliff detection, when multiple models share a camera and use the yolov5 model.

	2. Uses a message queue to store photo data to prevent frame loss and achieve efficient concurrency. In general, the photo data is collected at 25fps, and the total time for pre-processing, inference, and post-processing is 40ms (25fps), so the time for taking and storing messages is basically equal.

	3. Since the model has a 640*640 input and the camera has a 640 * 480 input, cv::imdecode is used for decoding, and the rgb format is input into the model, using rga for accelerated proportional scaling.

	4. Three models are set up with three independent threads for three sessions, which are independent of each other and do not interfere with each other.

Model Output:

	For the rk3588 yolov5 model output, when converting to onnx, the cat operation in the forward layer should be removed, and the model directly outputs three feature maps of 20 * 20, 40 * 40, and 80 * 80. Modifications are made in the yolo.py and export.py files accordingly.

Performance:

	One model occupies 1.2T of the NPU and 40% of the CPU (for pre-processing, inference, and post-processing for drawing frames), and the inference time is 20ms. The perf top -p command can be used to view the CPU usage rate and can be precise to the CPU usage rate of a specific function.
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

[机器人视频观看](https://github.com/dreamflyforever/quickrun/blob/main/video/machine.mp4)
<video controls>
  <source src="video/machine.mp4" type="video/mp4">
  Your browser does not support the video tag.
</video>

### license  
MIT by Jim
