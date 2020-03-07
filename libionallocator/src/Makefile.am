SDM_PATH := ${WORKSPACE}/hardware/qcom/display/sdm

AM_CPPFLAGS += "-I$(SDM_PATH)/include"
AM_CPPFLAGS += "-I$(SDM_PATH)/../libdebug"
AM_CPPFLAGS += "-I$(SDM_PATH)/../include"
AM_CPPFLAGS += "-I${WORKSPACE}/vendor/qcom/opensource/commonsys-intf/display/include/"

c_sources = alloc_interface.cpp \
            ion_alloc_impl.cpp

lib_LTLIBRARIES = libionallocator.la
libionallocator_la_CC = @CC@
libionallocator_la_SOURCES = $(c_sources)
libionallocator_la_CFLAGS = $(COMMON_CFLAGS)
libionallocator_la_CPPFLAGS = $(AM_CPPFLAGS) -DLOG_TAG=\"ION_ALLOCATOR\"
libionallocator_la_LIBADD = ../../libformatutils/src/libformatutils.la
libionallocator_la_LDFLAGS = -shared -avoid-version