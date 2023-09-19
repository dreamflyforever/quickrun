set -e

ROOT_PWD=$( cd "$( dirname $0 )" && cd -P "$( dirname "$SOURCE" )" && pwd )
INSTALL_DIR=${ROOT_PWD}/install

cd ${INSTALL_DIR}
./quickrun assets/yolov5s_logo_quan_0918.rknn assets/logo.jpg
cd -
