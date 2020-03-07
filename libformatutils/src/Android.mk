LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

SDM_PATH := hardware/qcom/display/sdm
SDM_COMP_PATH := vendor/qcom/opensource/display/sdm-composer

common_flags := -Wno-missing-field-initializers -Wno-unused-parameter
common_flags += -Wall -Werror

common_includes := $(SDM_PATH)/include
common_includes += $(SDM_COMP_PATH)/include
common_includes += $(SDM_COMP_PATH)/libformatutils/inc

LOCAL_MODULE                  := libformatutils
LOCAL_SANITIZE                := integer_overflow
LOCAL_VENDOR_MODULE           := true
LOCAL_MODULE_TAGS             := optional
LOCAL_C_INCLUDES              := $(common_includes)

LOCAL_ADDITIONAL_DEPENDENCIES := $(common_deps)
LOCAL_HEADER_LIBRARIES        := display_headers

LOCAL_CFLAGS                  := -DLOG_TAG=\"FORMATS\" $(common_flags) -fcolor-diagnostics
LOCAL_CLANG                   := true

LOCAL_SHARED_LIBRARIES        := libutils libcutils libc++

LOCAL_SRC_FILES               := formats.cpp

include $(BUILD_SHARED_LIBRARY)
