LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

SDM_PATH := hardware/qcom/display/sdm
SDM_COMP_PATH := vendor/qcom/opensource/display/sdm-composer

common_flags := -Wno-missing-field-initializers
common_flags += -Wall -Werror

common_includes := $(SDM_PATH)/include
common_includes += $(SDM_PATH)/../libdebug
common_includes += $(SDM_COMP_PATH)/include
common_includes += $(SDM_COMP_PATH)/libformatutils/inc

kernel_includes := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include

common_deps := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

LOCAL_MODULE                  := libsdmcomposer
LOCAL_SANITIZE                := integer_overflow
LOCAL_VENDOR_MODULE           := true
LOCAL_MODULE_TAGS             := optional
LOCAL_C_INCLUDES              := $(common_includes) \
                                 $(kernel_includes)
LOCAL_ADDITIONAL_DEPENDENCIES := $(common_deps)
LOCAL_HEADER_LIBRARIES        := display_headers

LOCAL_CFLAGS                  := -Wno-missing-field-initializers -Wno-unused-parameter \
                                 -DLOG_TAG=\"SDM-COMP\" $(common_flags) -fcolor-diagnostics
LOCAL_CLANG                   := true

LOCAL_SHARED_LIBRARIES        := libutils libcutils libsync libformatutils libionallocator \
                                 liblog libc++ libsdmcore libdisplaydebug libsdmutils

LOCAL_SRC_FILES               := sdm_comp_buffer_allocator.cpp \
                                 sdm_comp_buffer_sync_handler.cpp \
                                 sdm_comp_debugger.cpp \
                                 sdm_comp_display_builtin.cpp \
                                 sdm_comp_impl.cpp \
                                 sdm_comp_interface.cpp

include $(BUILD_SHARED_LIBRARY)
