SUBDIRS = MSVC

bin_PROGRAMS = modeltool

modeltool_SOURCES =	\
	model.h		\
	modeltool.cxx	\
	Q3BSP.cxx	\
	Q3BSP.h		\
	wavefrontOBJ.cxx\
	wavefrontOBJ.h
modeltool_LDFLAGS = -lcurl

AM_CPPFLAGS = $(CONF_CPPFLAGS)
AM_CFLAGS = $(CONF_CFLAGS)
AM_CXXFLAGS = $(CONF_CXXFLAGS)

MAINTAINERCLEANFILES = \
	Makefile.in

LDADD = \
	$(top_srcdir)/src/common/libCommon.la
