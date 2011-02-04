ifeq ($(BOARD_CAMERA_BUILD_FROM_SOURCE),true)
# When zero we link against libmmcamera; when 1, we dlopen libmmcamera.
DLOPEN_LIBMMCAMERA:=1

ifneq ($(BUILD_TINY_ANDROID),true)

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= QualcommCameraHardware.cpp

LOCAL_CFLAGS:= -DDLOPEN_LIBMMCAMERA=$(DLOPEN_LIBMMCAMERA)

LOCAL_C_INCLUDES+= \
	vendor/qcom/proprietary/mm-camera/common \
	vendor/qcom/proprietary/mm-camera/apps/appslib \
	external/jpeg \
	vendor/qcom/proprietary/mm-camera/jpeg/inc

LOCAL_SHARED_LIBRARIES:= libbinder libutils libcamera_client liblog

ifneq ($(DLOPEN_LIBMMCAMERA),1)
LOCAL_SHARED_LIBRARIES+= libmmcamera libmm-qcamera-tgt
else
LOCAL_SHARED_LIBRARIES+= libdl
endif

ifeq ($(BOARD_CAMERA_USE_GETBUFFERINFO),true)
LOCAL_CFLAGS += -DUSE_GETBUFFERINFO
endif

LOCAL_MODULE:= libcamera
include $(BUILD_SHARED_LIBRARY)

endif # BUILD_TINY_ANDROID
endif
