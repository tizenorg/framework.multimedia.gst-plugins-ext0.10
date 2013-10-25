plugin_LTLIBRARIES = libgstevaspixmapsink.la

libgstevaspixmapsink_la_SOURCES = evaspixmapsink.c
libgstevaspixmapsink_la_CFLAGS = $(GST_CFLAGS) $(X_CFLAGS) $(EFL_CFLAGS) $(MMTA_CFLAGS) \
	$(XFIXES_CFLAGS) $(DRI2PROTO_CFLAGS) $(DRI2_CFLAGS) $(X11_CFLAGS) $(XDAMAGE_CFLAGS) $(XV_CFLAGS) $(TBM_CFLAGS)
libgstevaspixmapsink_la_LIBADD = \
	$(GST_LIBS) -lgstvideo-$(GST_MAJORMINOR) -lgstinterfaces-$(GST_MAJORMINOR) \
	$(X_LIBS) $(XVIDEO_LIBS) $(XSHM_LIBS) $(LIBM) \
	$(EFL_LIBS) \
	$(MMTA_LIBS) \
	$(XFIXES_LIBS) $(DRI2PROTO_LIBS) $(DRI2_LIBS) $(X11_LIBS) $(XDAMAGE_LIBS) $(XV_LIBS) $(TBM_LIBS)
libgstevaspixmapsink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstxvimagesink_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = evaspixmapsink.h
