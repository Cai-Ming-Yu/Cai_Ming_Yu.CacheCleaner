LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(LOCAL_PATH)/cachecleaner.cpp  \
  $(wildcard $(LOCAL_PATH)/../thirdparty/yaml-cpp/src/*.cpp) \
  $(wildcard $(LOCAL_PATH)/../thirdparty/yaml-cpp/src/contrib/*.cpp)

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../ \
  $(LOCAL_PATH)/../thirdparty/yaml-cpp/include \
  $(LOCAL_PATH)/../thirdparty/CU-Utils/StringMatcher \
  $(LOCAL_PATH)/../thirdparty/CU-Utils/Logger

LOCAL_MODULE := CacheCleaner

LOCAL_MODULE_FILENAME := CacheCleaner

COMMAND += -static -s -Wl,--strip-all -O3 -fPIC -pipe -g0 -ffast-math -fno-builtin -frtti -fvisibility=hidden -fvisibility-inlines-hidden -fstack-protector-strong -fomit-frame-pointer -ffunction-sections -fdata-sections -fomit-frame-pointer -funroll-loops -fno-strict-aliasing -fpermissive -fomit-frame-pointer -funroll-loops -fno-strict-aliasing -fomit-frame-pointer -fexceptions

LOCAL_CFLAGS := $(COMMAND)
LOCAL_CPPFLAGS := -std=gnu++2b
LOCAL_LDFLAGS := $(COMMAND) -ldl

include $(BUILD_EXECUTABLE)