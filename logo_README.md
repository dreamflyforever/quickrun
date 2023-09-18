train: /home/jim/workspace/yolov5  command: python train.py --data train_logo/logo.yaml --weights '' --cfg yolov5s.yaml --img 640
dataset: /home/jim/workspace/yolov5/train_logo
export onnx: python export.py --weights last.pt --include  onnx
onnx2rknn: /home/jim/workspace/db01/RKNN_SDK/rknn-toolkit2/examples/onnx/yolov5 cmd: python test.py
