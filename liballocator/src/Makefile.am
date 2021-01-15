SDM_PATH := ${WORKSPACE}/hardware/qcom/display/sdm

AM_CPPFLAGS += "-I$(SDM_PATH)/include"
AM_CPPFLAGS += "-I$(SDM_PATH)/../libdebug"
AM_CPPFLAGS += "-I$(SDM_PATH)/../include"
AM_CPPFLAGS += "-I${WORKSPACE}/vendor/qcom/opensource/commonsys-intf/display/include/"
AM_CPPFLAGS += "-I${WORKSPACE}/system/memory/libdmabufheap/include/"

c_sources = alloc_interface.cpp \
            ion_alloc_impl.cpp \
            dma_buf_alloc_impl.cpp

lib_LTLIBRARIES = liballocator.la
liballocator_la_CC = @CC@
liballocator_la_SOURCES = $(c_sources)
liballocator_la_CFLAGS = $(COMMON_CFLAGS)
liballocator_la_CPPFLAGS = $(AM_CPPFLAGS) -DLOG_TAG=\"ION_ALLOCATOR\"
liballocator_la_LIBADD = ../../libformatutils/src/libformatutils.la
liballocator_la_LDFLAGS = -shared -avoid-version -ldmabufheap
