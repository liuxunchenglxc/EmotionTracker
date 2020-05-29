LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

OpenCV_INSTALL_MODULES := on
OpenCV_CAMERA_MODULES := off

OPENCV_LIB_TYPE := SHARED

ifeq ("$(wildcard $(OPENCV_MK_PATH))","")
include ../../../../native/jni/OpenCV.mk
else
include $(OPENCV_MK_PATH)
endif

LOCAL_MODULE := facetracker_cpp

LOCAL_SRC_FILES := com_calculator_liuxuncheng_emotiontracker_FaceTrackProccesor.cpp

LOCAL_LDLIBS +=  -lm -llog -landroid

include $(BUILD_SHARED_LIBRARY)