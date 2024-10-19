PROJECT(VeAI_WY)

CMAKE_MINIMUM_REQUIRED(VERSION 3.5)

# ===== Setp 1 : Set Cross Compiler Path =====

SET(BUILDROOT_CC "RV1126_RV1109_LINUX_SDK_V2.2.5.1_20230530/buildroot/output/rockchip_rv1126_rv1109/host/bin/")

SET(COMPILER_PATH "/home/xjt/_Workspace_/VOC/System/${BUILDROOT_CC}")

SET(CMAKE_C_COMPILER ${COMPILER_PATH}arm-linux-gnueabihf-gcc)
SET(CMAKE_CXX_COMPILER ${COMPILER_PATH}arm-linux-gnueabihf-g++)

# ===== Setp 2 : Set Flags =====

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s -O3 -lrt -pthread -fopenmp")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s -O3 -lrt -pthread -fopenmp")

# ===== Setp 3 : Set Application Name =====

SET(APP_NAME "veai_wy")

# ===== Setp 4 : Add Subdirectory =====

ADD_SUBDIRECTORY(src bin)