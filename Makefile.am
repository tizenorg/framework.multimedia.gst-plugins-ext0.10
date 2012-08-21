
ACLOCAL_AMFLAGS = -I common/m4

aclocaldir = $(datadir)/aclocal

SUBDIRS = common

if GST_EXT_USE_EXT_PIFFDEMUX
#SUBDIRS += piffdemux
endif

if GST_EXT_USE_EXT_ENCODEBIN
SUBDIRS += encodebin
endif

if GST_EXT_USE_EXT_AVSYSTEM
SUBDIRS += avsystem
endif

if GST_EXT_USE_EXT_EVASIMAGESINK
SUBDIRS += evasimagesink
endif

if GST_EXT_USE_EXT_DRMSRC
SUBDIRS += drmsrc
endif

if GST_EXT_USE_EXT_TOGGLE
SUBDIRS += toggle
endif

if GST_EXT_USE_EXT_AVSYSTEM
SUBDIRS += pdpushsrc
endif

if GST_EXT_USE_EXT_AUDIOTP
SUBDIRS += audiotp
endif

if GST_EXT_USE_EXT_SSDEMUX
#SUBDIRS += ssdemux
endif



DIST_SUBDIRS = common

if GST_EXT_USE_EXT_PIFFDEMUX
#DIST_SUBDIRS += piffdemux
endif

if GST_EXT_USE_EXT_ENCODEBIN
DIST_SUBDIRS += encodebin
endif

if GST_EXT_USE_EXT_AVSYSTEM
DIST_SUBDIRS += avsystem
endif

if GST_EXT_USE_EXT_EVASIMAGESINK
DIST_SUBDIRS += evasimagesink
endif

if GST_EXT_USE_EXT_DRMSRC
DIST_SUBDIRS += drmsrc
endif

if GST_EXT_USE_EXT_TOGGLE
DIST_SUBDIRS += toggle
endif

if GST_EXT_USE_EXT_SSDEMUX
#DIST_SUBDIRS += ssdemux
endif



EXTRA_DIST = \
	gstreamer.spec gstreamer.spec.in \
	configure.ac autogen.sh depcomp \
	MAINTAINERS ABOUT-NLS RELEASE gstreamer.doap \
	$(win32)

check: check-exports

