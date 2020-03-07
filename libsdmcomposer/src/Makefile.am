SDM_PATH := ${WORKSPACE}/hardware/qcom/display/sdm

AM_CPPFLAGS += "-I$(SDM_PATH)/include"
AM_CPPFLAGS += "-I$(SDM_PATH)/../libdebug"
AM_CPPFLAGS += "-I$(SDM_PATH)/../include"
AM_CPPFLAGS += "-I${WORKSPACE}/vendor/qcom/opensource/commonsys-intf/display/include/"

c_sources = sdm_comp_buffer_allocator.cpp \
            sdm_comp_buffer_sync_handler.cpp \
            sdm_comp_debugger.cpp \
            sdm_comp_display_builtin.cpp \
            sdm_comp_impl.cpp \
            sdm_comp_interface.cpp

lib_LTLIBRARIES = libsdmcomposer.la
libsdmcomposer_la_CC = @CC@
libsdmcomposer_la_SOURCES = $(c_sources)
libsdmcomposer_la_CFLAGS = $(COMMON_CFLAGS)
libsdmcomposer_la_CPPFLAGS = $(AM_CPPFLAGS) -DLOG_TAG=\"SDMCOMPOSER\"
libsdmcomposer_la_LIBADD := ../../libformatutils/src/libformatutils.la \
                            ../../libionallocator/src/libionallocator.la
libsdmcomposer_la_LDFLAGS := -shared -avoid-version -lsdmcore -lsdmutils