LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(LOCAL_PATH)/cachecleaner.cpp  \
  $(wildcard $(LOCAL_PATH)/../thirdparty/yaml-cpp/src/*.cpp) \
  $(wildcard $(LOCAL_PATH)/../thirdparty/yaml-cpp/src/contrib/*.cpp)

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../ \
  $(LOCAL_PATH)/../thirdparty/yaml-cpp/include \
  $(LOCAL_PATH)/../thirdparty/CU-Utils/Format \
  $(LOCAL_PATH)/../thirdparty/CU-Utils/StringMatcher \
  $(LOCAL_PATH)/../thirdparty/CU-Utils/Logger

LOCAL_MODULE := CacheCleaner

CMD := -static -Oz -fexceptions

LOCAL_CFLAGS := $(CMD)
LOCAL_CPPFLAGS := -std=c++17
LOCAL_LDFLAGS := $(CMD)

include $(BUILD_EXECUTABLE)