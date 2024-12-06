# **Quickrun**

Quickrun is a software designed for the efficient and high-concurrency deployment of multiple models on the RK3588 platform with RKNN.

---

## **Software Framework**

1. **Session-Based Design:**
   - Implements a session concept, allowing the definition of multiple sessions for different task requirements.
   - Example: Supports scenarios like charging pile detection, garbage classification, and cliff detection where multiple models share the same camera, e.g., using YOLOv5.

2. **Message Queue for Data Management:**
   - Uses a message queue to store photo data, preventing frame loss and ensuring efficient concurrency.
   - Photo data is collected at 25fps, with a total processing time (pre-processing, inference, post-processing) of 40ms per frame.

3. **Input Processing:**
   - Handles model input of `640x640` while the camera input is `640x480`.
   - Decodes the input using `cv::imdecode`, converts it to RGB format, and leverages RGA for accelerated proportional scaling.

4. **Threaded Model Execution:**
   - Three independent threads for three sessions to execute models concurrently without interference.

---

## **Model Output Customization**

- For RK3588 YOLOv5 model:
  - When converting to ONNX, remove the `cat` operation in the forward layer.
  - Configure the model to output three feature maps: `20x20`, `40x40`, and `80x80`.
  - Modify the necessary files: `yolo.py` and `export.py`.

---

## **Performance Overview**

- **Resource Usage:**
  - One model uses:
    - **1.2T** of the NPU.
    - **40%** of the CPU for pre-processing, inference, and post-processing (including frame drawing).

- **Inference Time:**
  - Achieves an inference time of **20ms**.

- **CPU Monitoring:**
  - Use the `perf top -p` command to view the CPU usage rate, down to specific functions.

---

## **Quick Start**

### **Compilation**

Run the following command to build the project:
```bash
bash build_rk3588_yolov5.sh
```
Testing

Run the following command to test the deployment:
```
bash test_rk3588_yolov5.sh
```
### Quickrun Deployment

Modify the following parameters in the code as per your project’s requirements:
```
#define OBJ_NAME_MAX_SIZE 16  
#define OBJ_NUMB_MAX_SIZE 64  
#define OBJ_CLASS_NUM 1      
#define NMS_THRESH 0.25       
#define BOX_THRESH 0.5 
```
## Robot Demo Video

View Robot Video on GitHub

[机器人视频观看](https://github.com/dreamflyforever/quickrun/blob/main/video/machine.mp4)
You can also watch the video below:

<source src="video/machine.mp4" type="video/mp4">
  Your browser does not support the video tag.
</video>


## License

Quickrun is licensed under the MIT License by Jim.
