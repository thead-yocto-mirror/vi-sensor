##
 # Copyright (C) 2020 Alibaba Group Holding Limited
##
VERBOSE=1

CONFIG_COMPILE_PLATFORM=RISCV

CONFIG_ISP_BUILD_TYPE=DEBUG
#CONFIG_ISP_BUILD_TYPE=RELEASE
CONFIG_ISP_VERSION=ISP8000L_V2008
CONFIG_ISP_LIB_BUILD_DIR=units/build
CONFIG_SENSOR_LIB_BUILD_DIR=build
CONFIG_ISP_RY_BUILD_TYPE=DEBUG
#CONFIG_ISP_RY_BUILD_TYPE=RELEASE
CONFIG_ISP_RY_VERSION=ISP8000_V2009
CONFIG_ISP_RY_LIB_BUILD_DIR=units_ry/build

CONFIG_DW200_CMODEL=off # on, off
CONFIG_DW200_V4L2=off # on, off
CONFIG_DW200_BUILD_DIR=dw200/build

CONFIG_DEC400_BUILD_DIR=dec400/build
CONFIG_VI_MONITOR_BUILD_DIR=vi_monitor/build
CONFIG_VI_MONITOR_TRACE=MONITOR_TRACE_ON
#CONFIG_VI_MONITOR_TRACE=MONITOR_TRACE_OFF

export CONFIG_HAL_PLATFORM=HAL_ALTERA
export COMPILE_PLATFORM=$(CONFIG_COMPILE_PLATFORM)

CONFIG_CMAKE_SENSOR_LIB_CMD:= \
    cmake -DCMAKE_BUILD_TYPE=$(CONFIG_ISP_BUILD_TYPE)       \
      -DISP_VERSION=$(CONFIG_ISP_VERSION)                   \
      -DSUBDEV_CHAR=1 -DHAL_PLATFORM=$(CONFIG_HAL_PLATFORM) \
      -DUSE_3AV2=1 \
      -DDUMMY_BUILD=0 \
      -DVVCAM_INC_PATH=$(VVCAM_INC_PATH) \
	  -DCMAKE_VERBOSE_MAKEFILE=ON\
      -Wno-dev ../drivers/

DIR_ISP_TARGET_BASE=bsp/isp
DIR_ISP_TARGET_LIB =bsp/isp/lib
DIR_ISP_TARGET_KO  =bsp/isp/ko
DIR_ISP_TARGET_TEST=bsp/isp/test
DIR_ISP_TARGET_SDK =bsp/isp/sdk

DIR_ISP_RY_TARGET_BASE=bsp/isp_ry
DIR_ISP_RY_TARGET_LIB =bsp/isp_ry/lib
DIR_ISP_RY_TARGET_KO  =bsp/isp_ry/ko
DIR_ISP_RY_TARGET_TEST=bsp/isp_ry/test
DIR_ISP_RY_TARGET_SDK =bsp/isp_ry/sdk

DIR_DW200_TARGET_BASE=bsp/dw200
DIR_DW200_TARGET_TEST=bsp/dw200/test
DIR_DW200_TARGET_DEMO=bsp/dw200/demo
DIR_DW200_TARGET_LIB=bsp/dw200/lib

DIR_DEC400_TARGET_BASE=bsp/dec400
DIR_DEC400_TARGET_TEST=bsp/dec400/test
DIR_DEC400_TARGET_LIB=bsp/dec400/lib

DIR_VI_MONITOR_TARGET_BASE=bsp/vi_monitor
DIR_VI_MONITOR_TARGET_TEST=bsp/vi_monitor/test
DIR_VI_MONITOR_TARGET_LIB=bsp/vi_monitor/lib

MODULE_NAME=ISP
BUILD_LOG_START="\033[47;30m>>> $(MODULE_NAME) $@ begin\033[0m"
BUILD_LOG_END  ="\033[47;30m<<< $(MODULE_NAME) $@ end\033[0m"

#
# Do a parallel build with multiple jobs, based on the number of CPUs online
# in this system: 'make -j8' on a 8-CPU system, etc.
#
# (To override it, run 'make JOBS=1' and similar.)
#
ifeq ($(JOBS),)
  JOBS := $(shell grep -c ^processor /proc/cpuinfo 2>/dev/null)
  ifeq ($(JOBS),)
    JOBS := 1
  endif
endif

all:    info sensor_lib \
		install_local_output install_rootfs
.PHONY: info sensor_lib \
		install_local_output install_rootfs \
        clean_sensor_lib \
        clean_output clean

info:
	@echo $(BUILD_LOG_START)
	@echo $(VVCAM_INC_PATH)
	@echo "  ====== Build Info from repo project ======"
	@echo "    BUILDROOT_DIR="$(BUILDROOT_DIR)
	@echo "    CROSS_COMPILE="$(CROSS_COMPILE)
	@echo "    LINUX_DIR="$(LINUX_DIR)
	@echo "    ARCH="$(ARCH)
	@echo "    BOARD_NAME="$(BOARD_NAME)
	@echo "    KERNEL_ID="$(KERNELVERSION)
	@echo "    KERNEL_DIR="$(LINUX_DIR)
	@echo "    INSTALL_DIR_ROOTFS="$(INSTALL_DIR_ROOTFS)
	@echo "    INSTALL_DIR_SDK="$(INSTALL_DIR_SDK)
	@echo "    DIR_MODULE_TOP="$(DIR_MODULE_TOP)
	@echo "  ====== Build configuration by settings ======"
	@echo "    COMPILE_PLATFORM="$(CONFIG_COMPILE_PLATFORM)
	@echo "    JOBS="$(JOBS)
	@echo "    ISP_BUILD_TYPE="$(CONFIG_ISP_BUILD_TYPE)
	@echo "    ISP_VERSION="$(CONFIG_ISP_VERSION)
	@echo "    ISP_LIB_BUILD_DIR="$(CONFIG_ISP_LIB_BUILD_DIR)
	@echo "    CMAKE_SENSOR_LIB_CMD=" $(CONFIG_CMAKE_SENSOR_LIB_CMD)
	@echo "    DW200_CMODEL="$(CONFIG_DW200_CMODEL)
	@echo "    DW200_V4L2="$(CONFIG_DW200_V4L2)
	@echo "    DW200_BUILD_DIR="$(CONFIG_DW200_BUILD_DIR)
	@echo "    DW200_CMAKE_CMD="$(CONFIG_DW200_CMAKE_CMD)
	@echo "    DEC400_BUILD_DIR="$(CONFIG_DEC400_BUILD_DIR)
	@echo "    DEC400_CMAKE_CMD="$(CONFIG_DEC400_CMAKE_CMD)
	@echo "    VI_MONITOR_BUILD_DIR="$(CONFIG_VI_MONITOR_BUILD_DIR)
	@echo "    VI_MONITOR_CMAKE_CMD="$(CONFIG_VI_MONITOR_CMAKE_CMD)
	@echo "    ISP_RY_BUILD_TYPE="$(CONFIG_ISP_RY_BUILD_TYPE)
	@echo "    ISP_RY_VERSION="$(CONFIG_ISP_RY_VERSION)
	@echo "    ISP_RY_LIB_BUILD_DIR="$(CONFIG_ISP_RY_LIB_BUILD_DIR)
	@echo "    CMAKE_ISP_RY_LIB_CMD=" $(CONFIG_CMAKE_ISP_RY_LIB_CMD)
	@echo $(BUILD_LOG_END)

sensor_lib: 
	mkdir -p $(CONFIG_SENSOR_LIB_BUILD_DIR); \
	cd $(CONFIG_SENSOR_LIB_BUILD_DIR);       \
	$(CONFIG_CMAKE_SENSOR_LIB_CMD);          \
	make -j$(JOBS);                   \

clean_sensor_lib:
	@echo $(BUILD_LOG_START)
	rm -rf $(CONFIG_SENSOR_LIB_BUILD_DIR)
	@echo $(BUILD_LOG_END)

install_local_output: sensor_lib
	@echo $(BUILD_LOG_START)
	@echo $(BUILD_LOG_END)

install_rootfs: install_local_output
	@echo $(BUILD_LOG_START)
	@echo $(BUILD_LOG_END)

clean_output:
	@echo $(BUILD_LOG_START)
	@echo $(BUILD_LOG_END)

clean_proprietories_include:
	@echo $(BUILD_LOG_START)
	@echo $(BUILD_LOG_END)

clean: clean_output clean_sensor_lib \
	clean_proprietories_include
