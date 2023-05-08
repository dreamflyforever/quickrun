set -e

ROOT_PWD=$( cd "$( dirname $0 )" && cd -P "$( dirname "$SOURCE" )" && pwd )
INSTALL_DIR=${ROOT_PWD}/install

cd ${INSTALL_DIR}
./rk3588_yolov5 assets/drp.rknn assets/drp.png
cd -
