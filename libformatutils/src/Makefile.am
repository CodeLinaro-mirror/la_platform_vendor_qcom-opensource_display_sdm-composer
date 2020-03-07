SDM_PATH := ${WORKSPACE}/hardware/qcom/display/sdm

AM_CPPFLAGS += "-I$(SDM_PATH)/include"
AM_CPPFLAGS += "-I${WORKSPACE}/vendor/qcom/opensource/commonsys-intf/display/include/"

c_sources = formats.cpp

lib_LTLIBRARIES = libformatutils.la
libformatutils_la_CC = @CC@
libformatutils_la_SOURCES = $(c_sources)
libformatutils_la_CFLAGS = $(COMMON_CFLAGS)
libformatutils_la_CPPFLAGS = $(AM_CPPFLAGS) -DLOG_TAG=\"FORMATUTILS\"
libformatutils_la_LDFLAGS = -shared -avoid-version