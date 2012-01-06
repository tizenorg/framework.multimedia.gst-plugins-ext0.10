
# plugindir is set in configure

plugin_LTLIBRARIES = libgstavsysaudiosrc.la

# sources used to compile this plug-in
libgstavsysaudiosrc_la_SOURCES = gstavsyssrc.c \
                                 gstavsysaudiosrc.c

libgstavsysaudiosrc_la_CFLAGS = $(GST_CFLAGS) \
                           $(GST_BASE_CFLAGS) \
                           $(AVSYSTEM_CFLAGS) \
                           $(GST_VIDEO_FLAGS) \
                           -I$(includedir)/mmf \
                           $(MMTA_CFLAGS) \
                           $(GST_AUDIO_CFLAGS) \
                           $(VCONF_CFLAGS) \
                           $(AVSYSAUDIO_CFLAGS)

libgstavsysaudiosrc_la_LIBADD = $(GST_LIBS) \
                           $(GST_BASE_LIBS) \
                           $(DATACOMLIB_LIBS) \
                           $(HTTPLIB_LIBS) \
                           $(AVSYSTEM_LIBS) \
                           $(GST_VIDEO_LIBS) \
                           -lgstaudio-0.10 \
                           $(MMTA_LIBS) \
                           $(GST_AUDIO_LIBS) \
                           -ldl \
                           $(VCONF_LIBS) \
                           $(AVSYSAUDIO_LIBS)

libgstavsysaudiosrc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

if IS_I386
libgstavsysaudiosrc_la_CFLAGS += -DI386_SIMULATOR
else
endif

plugin_LTLIBRARIES += libgstavsyssink.la

## sources used to compile this plug-in
libgstavsyssink_la_SOURCES = gstavsyssink.c \
			 gstavsysmemsink.c

libgstavsyssink_la_CFLAGS = $(GST_CFLAGS) $(GST_BASE_CFLAGS) $(AVSYSVIDEO_CFLAGS) $(AVSYSTEM_CFLAGS) -I$(includedir)/mmf
libgstavsyssink_la_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS) $(DATACOMLIB_LIBS) $(HTTPLIB_LIBS) $(AVSYSVIDEO_LIBS) $(AVSYSTEM_LIBS) $(GST_VIDEO_LIBS) -lgstaudio-0.10 -ldl
libgstavsyssink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)


libgstavsyssink_la_SOURCES += gstavsysaudiosink.c
libgstavsyssink_la_CFLAGS  += $(AVSYSAUDIO_CFLAGS)
libgstavsyssink_la_LIBADD  += $(AVSYSAUDIO_LIBS)

if IS_I386
libgstavsyssink_la_CFLAGS += -DI386_SIMULATOR
else
endif


