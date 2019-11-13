#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

CONTAINER_IMG_NAME=$1
CMD=$2
APP_DIR=`dirname ${0}`
ENVSH=${APP_DIR}/ubuntu/dpdk/env.sh

# Include env.sh
if [ -e ${ENVSH} ];then
  . ${ENVSH}
else
  _build_env='./build/main.py --only-envsh'
  echo "[Error] ${ENVSH} does not exist!"
  echo "You have to build image or run '${_build_env}' to create it."
  exit
fi

if [ ! $2 ]; then
  echo "usage: $0 [IMAGE] [COMMAND]"
  exit
fi

#cd ${APP_DIR}; \
sudo docker run -i -t \
	-e http_proxy=${http_proxy} \
	-e https_proxy=${https_proxy} \
	-e HTTP_PROXY=${http_proxy} \
	-e HTTPS_PROXY=${https_proxy} \
	-e RTE_SDK=${RTE_SDK} \
	-e RTE_TARGET=${RTE_TARGET} \
	${CONTAINER_IMG_NAME} \
  ${CMD}
