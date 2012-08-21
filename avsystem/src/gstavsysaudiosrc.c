/*
 * avsystem
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <gst/gst.h>
#include <gst/gstutils.h>
//#include <mm_debug.h>

#include <sys/time.h>	/*gettimeofday*/
#include <unistd.h>

#include "gstavsysaudiosrc.h"

/**
 * Define for Release
 *
 * _ENABLE_FAKE_READ:		enable fake read instead of avsys_audio_read(). (default: 0)
 */
#define	_ENABLE_FAKE_READ		0
//#define 	USE_GPT8


#define _MIN_RATE		8000
#define _MAX_RATE		48000
#define _MIN_CHANNEL		1
#define _MAX_CHANNEL		2


#define DEFAULT_GPT8_FREQ 26

#if _ENABLE_FAKE_READ
static long g_delay_time;
#endif

#define DEFAULT_MEDIACALL_MODE	AVSYS_AUDIO_ECHO_MODE_NONE
#define DEFAULT_AUDIO_LATENCY	AVSYSAUDIOSRC_LATENCY_MID

GST_DEBUG_CATEGORY_EXTERN (avsystem_src_debug);
#define GST_CAT_DEFAULT avsystem_src_debug

/* elementfactory information */
static const GstElementDetails gst_avsysaudiosrc_details =
	GST_ELEMENT_DETAILS ("AV system source",
						 "Source/Audio",
						 "Read from a AV system audio in",
						 "Samsung Electronics co., ltd.");


enum {
	PROP_0,
#if 0
#if !defined(I386_SIMULATOR)
    PROP_AUDIO_MEDIA_CALL,
#endif
#endif
    PROP_AUDIO_LATENCY,
};

enum {
	CAPTURE_UNCORK = 0,
	CAPTURE_CORK,
};

GType
gst_avsysaudiosrc_audio_latency_get_type (void)
{
  static GType capture_audio_latency_type = 0;
  static const GEnumValue capture_audio_latency[] = {
    {AVSYSAUDIOSRC_LATENCY_LOW, "Set capture latency as low", "low"},
    {AVSYSAUDIOSRC_LATENCY_MID, "Set capture latency as mid", "mid"},
    {AVSYSAUDIOSRC_LATENCY_HIGH, "Set capture latency as high", "high"},
    {0, NULL, NULL},
  };

  if (!capture_audio_latency_type) {
	  capture_audio_latency_type =
        g_enum_register_static ("GstAvsysAudioSrcAudioLatency", capture_audio_latency);
  }
  return capture_audio_latency_type;
}


GST_BOILERPLATE (GstAvsysAudioSrc, gst_avsysaudiosrc, GstAudioSrc, GST_TYPE_AUDIO_SRC);

#if _ENABLE_FAKE_READ
static unsigned long	fake_delay (unsigned long usec);
#endif

static guint gst_avsysaudiosrc_delay (GstAudioSrc *asrc);

static void		gst_avsysaudiosrc_finalize		(GObject * object);
static void		gst_avsysaudiosrc_set_property	(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void		gst_avsysaudiosrc_get_property	(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);

static GstCaps*	gst_avsysaudiosrc_getcaps	(GstBaseSrc * bsrc);

static gboolean	gst_avsysaudiosrc_open		(GstAudioSrc * asrc);
static gboolean	gst_avsysaudiosrc_prepare	(GstAudioSrc * asrc, GstRingBufferSpec * spec);
static gboolean	gst_avsysaudiosrc_unprepare	(GstAudioSrc * asrc);
static gboolean	gst_avsysaudiosrc_close		(GstAudioSrc * asrc);
static guint		gst_avsysaudiosrc_read		(GstAudioSrc * asrc, gpointer data, guint length);
static void		gst_avsysaudiosrc_reset		(GstAudioSrc * asrc);
static gboolean	gst_avsysaudiosrc_avsys_close	(GstAvsysAudioSrc *src);
static gboolean	gst_avsysaudiosrc_avsys_open	(GstAvsysAudioSrc *src);
static gboolean	gst_avsysaudiosrc_avsys_cork	(GstAvsysAudioSrc *avsysaudiosrc, int cork);
static gboolean	gst_avsysaudiosrc_avsys_start	(GstAvsysAudioSrc *src);
static gboolean	gst_avsysaudiosrc_avsys_stop	(GstAvsysAudioSrc *src);
#if defined(_USE_CAPS_)
static GstCaps *gst_avsysaudiosrc_detect_rates (GstObject * obj, avsys_pcm_hw_params_t * hw_params, GstCaps * in_caps);
static GstCaps *gst_avsysaudiosrc_detect_channels (GstObject * obj,avsys_pcm_hw_params_t * hw_params,  GstCaps * in_caps);
static GstCaps *gst_avsysaudiosrc_detect_formats (GstObject * obj,avsys_pcm_hw_params_t * hw_params,  GstCaps * in_caps);
static GstCaps *gst_avsysaudiosrc_probe_supported_formats (GstObject * obj, avsys_handle_t handle, const GstCaps * template_caps);
#endif
static GstStateChangeReturn gst_avsys_src_change_state (GstElement * element, GstStateChange transition) ;



/* AvsysAudioSrc signals and args */
enum {
	LAST_SIGNAL
};

# define AVSYS_AUDIO_SRC_FACTORY_ENDIANNESS   "LITTLE_ENDIAN"

static GstStaticPadTemplate avsysaudiosrc_src_factory =
	GST_STATIC_PAD_TEMPLATE ("src",
								GST_PAD_SRC,
								GST_PAD_ALWAYS,
								GST_STATIC_CAPS (
									"audio/x-raw-int, "
									"endianness = (int) { " AVSYS_AUDIO_SRC_FACTORY_ENDIANNESS " }, "
                             "signed = (boolean) { TRUE }, "
									"width = (int) 16, "
									"depth = (int) 16, "
                             "rate = (int) [ 8000, 48000 ], "
                             "channels = (int) [ 1, 2 ]; "
									"audio/x-raw-int, "
                             "signed = (boolean) { FALSE }, "
									"width = (int) 8, "
									"depth = (int) 8, "
                             "rate = (int) [ 8000, 48000 ], "
                             "channels = (int) [ 1, 2 ]")
							);

static inline guint _time_to_sample(GstAvsysAudioSrc *avsyssrc,  GstClockTime diff)
{
	guint result = 0;
	// sample rate : asrc->audio_param.samplerate
	result =(GST_TIME_AS_USECONDS(diff) * avsyssrc->audio_param.samplerate)/1000000;
	return result;
}


static void
gst_avsysaudiosrc_finalize (GObject * object)
{
    GstAvsysAudioSrc *src = NULL;


    src = GST_AVSYS_AUDIO_SRC (object);
	g_mutex_free (src->avsysaudio_lock);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_avsysaudiosrc_base_init (gpointer g_class)
{
	GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

	gst_element_class_set_details (element_class, &gst_avsysaudiosrc_details);

	gst_element_class_add_pad_template (element_class,
										gst_static_pad_template_get (&avsysaudiosrc_src_factory));
}

static void
gst_avsysaudiosrc_class_init (GstAvsysAudioSrcClass * klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;
	GstBaseSrcClass *gstbasesrc_class;
	GstBaseAudioSrcClass *gstbaseaudiosrc_class;
	GstAudioSrcClass *gstaudiosrc_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;
	gstbasesrc_class = (GstBaseSrcClass *) klass;
	gstbaseaudiosrc_class = (GstBaseAudioSrcClass *) klass;
	gstaudiosrc_class = (GstAudioSrcClass *) klass;

	gstelement_class->change_state = GST_DEBUG_FUNCPTR (gst_avsys_src_change_state);

    parent_class = g_type_class_peek_parent (klass);

	gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_avsysaudiosrc_finalize);
	gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_avsysaudiosrc_get_property);
	gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_avsysaudiosrc_set_property);
#if defined(_USE_CAPS_)
	gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_avsysaudiosrc_getcaps);
#endif
	gstaudiosrc_class->open = GST_DEBUG_FUNCPTR (gst_avsysaudiosrc_open);
	gstaudiosrc_class->prepare = GST_DEBUG_FUNCPTR (gst_avsysaudiosrc_prepare);
	gstaudiosrc_class->unprepare = GST_DEBUG_FUNCPTR (gst_avsysaudiosrc_unprepare);
	gstaudiosrc_class->close = GST_DEBUG_FUNCPTR (gst_avsysaudiosrc_close);
	gstaudiosrc_class->read = GST_DEBUG_FUNCPTR (gst_avsysaudiosrc_read);
	gstaudiosrc_class->delay = GST_DEBUG_FUNCPTR (gst_avsysaudiosrc_delay);
	gstaudiosrc_class->reset = GST_DEBUG_FUNCPTR (gst_avsysaudiosrc_reset);

	g_object_class_install_property (gobject_class ,PROP_AUDIO_LATENCY,
			g_param_spec_enum("latency", "Audio Latency",
					"Audio latency",
					GST_AVSYS_AUDIO_SRC_LATENCY, DEFAULT_AUDIO_LATENCY,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS ));
}



/**
 *@note useful range 0 - 1000000 microsecond
 */
#if _ENABLE_FAKE_READ
static unsigned long
fake_delay (unsigned long usec)
{
	struct timeval s_time;
	struct timeval c_time;
	long s_sec  = 0L;
	long s_usec = 0L;
	long c_sec  = 0L;
	long c_usec = 0L;
	long t_sec  = 0L;
	long t_usec = 0L;
	unsigned long total_usec = 0UL;
	unsigned long t = 0UL;

	if (usec == 0UL) {
		usleep (0);
		return 0;
	}

	/*get start time*/
	if (gettimeofday ((struct timeval *)&s_time, NULL) == 0) {
		s_sec  = s_time.tv_sec;
		s_usec = s_time.tv_usec;
	} else {
		return 0;
	}

	for (;;) {
		/*get current time*/
		if (gettimeofday ((struct timeval *)&c_time, NULL) == 0) {
			c_sec  = c_time.tv_sec;
			c_usec = c_time.tv_usec;
		} else {
			return 0;
		}

		/*get elasped sec*/
		t_sec = c_sec - s_sec;

		/*get elapsed usec*/
		if ((s_usec) > (c_usec)) {
			t_usec = 1000000L - (s_usec) + (c_usec);
			t_sec--;
		} else {
			t_usec = (c_usec) - (s_usec);
		}

		/*get total elapsed time*/
		total_usec = (t_sec * 1000000UL) + t_usec;

		t = usec - total_usec;

		if (total_usec >= usec) {
			break;
		} else {
			if (t > 10000UL) {
				/*this function does not work in precision*/
				usleep (1);
			}
		}
	}

	return total_usec;
}
#endif

static guint
gst_avsysaudiosrc_delay (GstAudioSrc *asrc)
{
	GstAvsysAudioSrc	*avsys_audio = NULL;
	int					delay;
	guint				retValue = 0;

	avsys_audio = GST_AVSYS_AUDIO_SRC (asrc);
	if(AVSYS_STATE_SUCCESS == avsys_audio_delay(avsys_audio->audio_handle, &delay))
		retValue = delay;

	return retValue;
}



static void
gst_avsysaudiosrc_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
    GstAvsysAudioSrc *src = NULL;
//    gint			getvalue = 0;
	src = GST_AVSYS_AUDIO_SRC (object);

	if (src->cached_caps == NULL)
	{
		switch (prop_id)
		{
        case PROP_AUDIO_LATENCY:
        	src->latency = g_value_get_enum(value);
			break;
        default:
        	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        	break;
		}
	}
}

static void
gst_avsysaudiosrc_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
	GstAvsysAudioSrc *src;

	src = GST_AVSYS_AUDIO_SRC (object);

    switch (prop_id)
    {
	case PROP_AUDIO_LATENCY:
		g_value_set_enum(value, src->latency);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gst_avsysaudiosrc_init (GstAvsysAudioSrc * avsysaudiosrc, GstAvsysAudioSrcClass * g_class)
{
	GST_DEBUG_OBJECT (avsysaudiosrc, "initializing");

	avsysaudiosrc->cached_caps = NULL;
	avsysaudiosrc->audio_handle = (avsys_handle_t)-1;
	avsysaudiosrc->buffer_size = 0;
	avsysaudiosrc->avsysaudio_lock = g_mutex_new ();
	avsysaudiosrc->latency = DEFAULT_AUDIO_LATENCY;
}

#if defined(_USE_CAPS_)
static GstCaps *
gst_avsysaudiosrc_getcaps (GstBaseSrc * bsrc)
{
    GstElementClass *element_class;
    GstPadTemplate *pad_template;
	GstAvsysAudioSrc *src;
    GstCaps *caps = NULL;

  	src = GST_AVSYS_AUDIO_SRC (bsrc);

    if(src->audio_handle==(avsys_handle_t)-1){
    	GST_DEBUG_OBJECT(src,"device not open, using template caps");
    	return NULL;
    }

    if (src->cached_caps)
	{
		GST_LOG_OBJECT (src, "Returning cached caps");
            return gst_caps_ref(src->cached_caps);
	}
	element_class = GST_ELEMENT_GET_CLASS (src);
	pad_template = gst_element_class_get_pad_template (element_class, "src");
	g_return_val_if_fail (pad_template != NULL, NULL);

	caps = gst_avsysaudiosrc_probe_supported_formats (GST_OBJECT (src), src->audio_handle,
													gst_pad_template_get_caps (pad_template));

	if (caps) {
		src->cached_caps = gst_caps_ref (caps);
	}

	GST_INFO_OBJECT (src, "returning caps %" GST_PTR_FORMAT, caps);

	return caps;
}
#endif
static gboolean
gst_avsysaudiosrc_open (GstAudioSrc * asrc)
{
	return TRUE;
}

static gboolean
avsysaudiosrc_parse_spec (GstAvsysAudioSrc *avsys_audio, GstRingBufferSpec * spec)
{
    /* Check param */
    if (spec->type != GST_BUFTYPE_LINEAR ||
        spec->channels > 2 || spec->channels < 1 ||
        !(spec->format == GST_S8 || spec->format == GST_S16_LE) )
        return FALSE;

    switch(spec->format)
    {
    case GST_S8:
        avsys_audio->audio_param.format = AVSYS_AUDIO_FORMAT_8BIT;
        avsys_audio->bytes_per_sample = 1;
        break;
    case GST_S16_LE:
        avsys_audio->audio_param.format = AVSYS_AUDIO_FORMAT_16BIT;
        avsys_audio->bytes_per_sample = 2;
        break;
    default:
    	GST_DEBUG_OBJECT(avsys_audio, "Only support S8, S16LE format");
        return FALSE;
    }

    // set audio parameter for avsys audio open

   	switch(avsys_audio->latency)
   	{
   	case AVSYSAUDIOSRC_LATENCY_LOW:
   		avsys_audio->audio_param.mode = AVSYS_AUDIO_MODE_INPUT_LOW_LATENCY;
   		break;
   	case AVSYSAUDIOSRC_LATENCY_MID:
   		avsys_audio->audio_param.mode = AVSYS_AUDIO_MODE_INPUT;
   		break;
   	case AVSYSAUDIOSRC_LATENCY_HIGH:
   		avsys_audio->audio_param.mode = AVSYS_AUDIO_MODE_INPUT_HIGH_LATENCY;
   		break;
   	default:
   		break;
   	}

    avsys_audio->audio_param.priority = 0;
    avsys_audio->audio_param.samplerate = spec->rate;
    avsys_audio->audio_param.channels = spec->channels;

    if(spec->channels == 2)
    	avsys_audio->bytes_per_sample *= 2;
    else if(spec->channels > 2)
    {
    	GST_ERROR_OBJECT(avsys_audio,"Unsupported channel number %d", spec->channels);
    	return FALSE;
    }

    return TRUE;
}

static gboolean
gst_avsysaudiosrc_prepare (GstAudioSrc * asrc, GstRingBufferSpec * spec)
{
    GstAvsysAudioSrc *avsysaudiosrc = NULL;
	guint	p_time = 0, b_time = 0;

	avsysaudiosrc = GST_AVSYS_AUDIO_SRC (asrc);

    if (!avsysaudiosrc_parse_spec(avsysaudiosrc, spec))
    {
    	GST_ERROR("avsysaudiosrc_parse_spec failed");
        return FALSE;
    }

	/*open avsys audio*/
    if (!gst_avsysaudiosrc_avsys_open (avsysaudiosrc))
    {
    	GST_ERROR("gst_avsysaudiosrc_avsys_open failed");
        return FALSE;
    }

    /* Ring buffer size */
    if (AVSYS_STATE_SUCCESS ==
     	avsys_audio_get_period_buffer_time(avsysaudiosrc->audio_handle, &p_time, &b_time))
    {
     	if(p_time == 0 || b_time == 0)
     		return FALSE;

     	spec->latency_time = (guint64)p_time;
     	spec->buffer_time = (guint64)b_time;
	}
	else
	{
    	 return FALSE;
	}
	spec->segsize = avsysaudiosrc->buffer_size;
	spec->segtotal = (b_time / p_time) + (((b_time % p_time)/p_time > 0.5) ? 1: 0);
	spec->segtotal += 2;

     GST_INFO_OBJECT(avsysaudiosrc, "audio buffer spec : latency_time(%llu), buffer_time(%llu), segtotal(%d), segsize(%d)\n",
 							spec->latency_time, spec->buffer_time, spec->segtotal, spec->segsize);


    return TRUE;
}

static gboolean
gst_avsysaudiosrc_unprepare (GstAudioSrc * asrc)
{
    GstAvsysAudioSrc *avsysaudiosrc = NULL;
    gboolean ret = TRUE;

    avsysaudiosrc = GST_AVSYS_AUDIO_SRC (asrc);

	/*close*/
    GST_AVSYS_AUDIO_SRC_LOCK (avsysaudiosrc);

    if(!gst_avsysaudiosrc_avsys_close(avsysaudiosrc))
    {
    	GST_ERROR_OBJECT(avsysaudiosrc, "gst_avsysaudiosrc_avsys_close failed");
    	ret = FALSE;
    }
    GST_AVSYS_AUDIO_SRC_UNLOCK (asrc);



	return ret;
}

static gboolean
gst_avsysaudiosrc_close (GstAudioSrc * asrc)
{
    GstAvsysAudioSrc *avsysaudiosrc = NULL;

    avsysaudiosrc = GST_AVSYS_AUDIO_SRC (asrc);
	gst_caps_replace (&avsysaudiosrc->cached_caps, NULL);

	return TRUE;
}

static void
gst_avsysaudiosrc_reset (GstAudioSrc * asrc)
{
	GstAvsysAudioSrc *avsys_audio = NULL;

    avsys_audio = GST_AVSYS_AUDIO_SRC (asrc);
    GST_AVSYS_AUDIO_SRC_LOCK (asrc);

    if(AVSYS_STATE_SUCCESS != avsys_audio_reset(avsys_audio->audio_handle))
    {
    	GST_ERROR_OBJECT (avsys_audio, "avsys-reset: internal error: ");
    }

    GST_AVSYS_AUDIO_SRC_UNLOCK (asrc);

    return;
}


static GstStateChangeReturn
gst_avsys_src_change_state (GstElement * element, GstStateChange transition)
{
	GstAvsysAudioSrc *avsys = NULL;
	GstStateChangeReturn ret ;

	avsys = GST_AVSYS_AUDIO_SRC (element);
	GST_DEBUG("gst_avsys_src_change_state");

	switch (transition)
	{
		case GST_STATE_CHANGE_NULL_TO_READY:
		{
			GST_DEBUG ("GST_STATE_CHANGE_NULL_TO_READY\n") ;
			break ;
		}
		case GST_STATE_CHANGE_READY_TO_PAUSED:
		{
			GST_DEBUG ("GST_STATE_CHANGE_READY_TO_PAUSED\n") ;
			break ;
		}
		case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
		{
			GST_DEBUG ("GST_STATE_CHANGE_PAUSED_TO_PLAYING\n") ;
			/* Capture Start */
			if (!gst_avsysaudiosrc_avsys_start (avsys)) {
				GST_ERROR("gst_avsysaudiosrc_avsys_start failed");
			}
			break ;
		}
		default:
			break ;
	}

	ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

	GST_DEBUG ("After parent_class->change_state...\n") ;
	switch (transition)
	{
		case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
		{
			GST_DEBUG ("GST_STATE_CHANGE_PLAYING_TO_PAUSED\n") ;
			/* Capture Stop */
			if (!gst_avsysaudiosrc_avsys_stop (avsys)) {
				GST_ERROR("gst_avsysaudiosrc_avsys_stop failed");
			}
			break ;
		}
		case GST_STATE_CHANGE_PAUSED_TO_READY:
		{
			GST_DEBUG ("GST_STATE_CHANGE_PAUSED_TO_READY\n") ;
			break ;
		}
		case GST_STATE_CHANGE_READY_TO_NULL:
		{
			GST_DEBUG ("GST_STATE_CHANGE_READY_TO_NULL\n") ;
			break ;
		}
		default:
			break ;
	}

	return ret;
}



static guint
gst_avsysaudiosrc_read (GstAudioSrc * asrc, gpointer data, guint length)
{
    GstAvsysAudioSrc	*avsysaudiosrc = NULL;
	gint				readed = 0;
	gpointer			ptr = NULL;
	guint				used_length = 0;

	avsysaudiosrc = GST_AVSYS_AUDIO_SRC (asrc);

	ptr = data;

    GST_AVSYS_AUDIO_SRC_LOCK (avsysaudiosrc);
#if _ENABLE_FAKE_READ
	readed = avsysaudiosrc->buffer_size;
	memset (ptr, 10, avsysaudiosrc->buffer_size);	/*maybe can't hear*/
	fake_delay (1000000UL / g_delay_time);
#else
	ptr = data;
	readed = avsys_audio_read (avsysaudiosrc->audio_handle, ptr, length);
	if (readed < 0)
		goto _READ_ERROR;
#endif //_ENABLE_FAKE_READ
	GST_AVSYS_AUDIO_SRC_UNLOCK (avsysaudiosrc);

	return readed;

_READ_ERROR:
	{
		GST_AVSYS_AUDIO_SRC_UNLOCK (asrc);
		return length;	/* skip one period */
	}
}

static gboolean
gst_avsysaudiosrc_avsys_cork (GstAvsysAudioSrc *avsysaudiosrc, int cork)
{
	int avsys_result = avsys_audio_cork (avsysaudiosrc->audio_handle, cork);
	if (avsys_result != AVSYS_STATE_SUCCESS) {
		GST_ERROR_OBJECT(avsysaudiosrc, "avsys_audio_cork() error. [0x%x]\n", avsys_result);
		return FALSE;
	}
	return TRUE;
}

static gboolean
gst_avsysaudiosrc_avsys_start (GstAvsysAudioSrc *avsysaudiosrc)
{
	gboolean result;

	GST_AVSYS_AUDIO_SRC_LOCK (avsysaudiosrc);
	result = gst_avsysaudiosrc_avsys_cork(avsysaudiosrc, CAPTURE_UNCORK);
	GST_AVSYS_AUDIO_SRC_UNLOCK (avsysaudiosrc);

	return result;
}

static gboolean
gst_avsysaudiosrc_avsys_stop (GstAvsysAudioSrc *avsysaudiosrc)
{
	gboolean result;

	GST_AVSYS_AUDIO_SRC_LOCK (avsysaudiosrc);
	result = gst_avsysaudiosrc_avsys_cork(avsysaudiosrc, CAPTURE_CORK);
	GST_AVSYS_AUDIO_SRC_UNLOCK (avsysaudiosrc);

	return result;
}

static gboolean
gst_avsysaudiosrc_avsys_open (GstAvsysAudioSrc *avsysaudiosrc)
{
	int avsys_result = 0;

	GST_AVSYS_AUDIO_SRC_LOCK (avsysaudiosrc);

	avsys_result = avsys_audio_open(&avsysaudiosrc->audio_param, &avsysaudiosrc->audio_handle, &avsysaudiosrc->buffer_size);

	if (avsys_result != AVSYS_STATE_SUCCESS) {
		GST_ERROR_OBJECT(avsysaudiosrc, "avsys_audio_open() error. [0x%x]\n", avsys_result);
		GST_AVSYS_AUDIO_SRC_UNLOCK (avsysaudiosrc);
		return FALSE;
	}

#if _ENABLE_FAKE_READ
	g_delay_time = (unsigned long)((avsysaudiosrc->samplerate * (avsysaudiosrc->format / 8) * avsysaudiosrc->channels) / avsysaudiosrc->buffer_size);
#endif

	GST_AVSYS_AUDIO_SRC_UNLOCK (avsysaudiosrc);
	return TRUE;
}

static gboolean
gst_avsysaudiosrc_avsys_close(GstAvsysAudioSrc *src)
{
	int ret;

    if (src->audio_handle != (avsys_handle_t)-1)
    {
		/*close avsys audio*/
		ret = avsys_audio_close (src->audio_handle);
        if (AVSYS_FAIL(ret))
        {
        	GST_ERROR_OBJECT(src, "avsys_audio_close() error 0x%x", ret);
			return FALSE;
		}
        src->audio_handle = (avsys_handle_t)-1;
        GST_DEBUG_OBJECT(src, "AVsys audio handle closed");
	}
    else
    {
    	GST_WARNING_OBJECT(src,"avsys audio handle has already closed");
    }

	return TRUE;
}
#if defined(_USE_CAPS_)
static GstCaps *
gst_avsysaudiosrc_detect_rates(GstObject *obj,
		avsys_pcm_hw_params_t *hw_params, GstCaps *in_caps)
{
	GstCaps *caps;
	guint min, max;
	gint err, dir, min_rate, max_rate, i;

	GST_LOG_OBJECT(obj, "probing sample rates ...");

	if ((err = avsys_pcm_hw_params_get_rate_min(hw_params, &min, &dir)) < 0)
		goto min_rate_err;

	if ((err = avsys_pcm_hw_params_get_rate_max(hw_params, &max, &dir)) < 0)
		goto max_rate_err;

	min_rate = min;
	max_rate = max;

	if (min_rate < _MIN_RATE)
		min_rate = _MIN_RATE; /* random 'sensible minimum' */

	if (max_rate <= 0)
		max_rate = _MAX_RATE; /* or maybe just use 192400 or so? */
	else if (max_rate > 0 && max_rate < _MIN_RATE)
		max_rate = MAX (_MIN_RATE, min_rate);

	GST_DEBUG_OBJECT(obj, "Min. rate = %u (%d)", min_rate, min);
	GST_DEBUG_OBJECT(obj, "Max. rate = %u (%d)", max_rate, max);

	caps = gst_caps_make_writable(in_caps);

	for (i = 0; i < gst_caps_get_size(caps); ++i)
	{
		GstStructure *s;

		s = gst_caps_get_structure(caps, i);
		if (min_rate == max_rate)
		{
			gst_structure_set(s, "rate", G_TYPE_INT, min_rate, NULL);
		}
		else
		{
			gst_structure_set(s, "rate", GST_TYPE_INT_RANGE, min_rate,
					max_rate, NULL);
		}
	}

	return caps;
	/* ERRORS */
	min_rate_err:
	{
		GST_ERROR_OBJECT(obj, "failed to query minimum sample rate");
		gst_caps_unref(in_caps);
		return NULL;
	}
	max_rate_err:
	{
		GST_ERROR_OBJECT(obj, "failed to query maximum sample rate");
		gst_caps_unref(in_caps);
		return NULL;
	}
}


static GstCaps *
gst_avsysaudiosrc_detect_formats(GstObject * obj,
		avsys_pcm_hw_params_t * hw_params, GstCaps * in_caps)
{
	avsys_pcm_format_mask_t *mask;
	GstStructure *s;
	GstCaps *caps;
	gint i;

	avsys_pcm_format_mask_malloc(&mask);
	avsys_pcm_hw_params_get_format_mask(hw_params, mask);

	caps = gst_caps_new_empty();

	for (i = 0; i < gst_caps_get_size(in_caps); ++i)
	{
		GstStructure *scopy;
		//gint w;
		gint width = 0, depth = 0;
		gint sndformat;

		s = gst_caps_get_structure(in_caps, i);
		if (!gst_structure_has_name(s, "audio/x-raw-int"))
		{
			GST_WARNING_OBJECT(obj, "skipping non-int format");
			continue;
		}
		if (!gst_structure_get_int(s, "width", &width)
				|| !gst_structure_get_int(s, "depth", &depth))
			continue;

		GST_DEBUG_OBJECT(obj, "width = %d height = %d", width, depth);
		if (width == 8)
			sndformat = 0;//SND_PCM_FORMAT_S8
		else
			//width==16
			sndformat = 2; //SND_PCM_FORMAT_S16_LE
		if (avsys_pcm_format_mask_test(mask, sndformat))
		{ //must be implemented
			/* template contains { true, false } or just one, leave it as it is */
			scopy = gst_structure_copy(s);

		}
		else
		{
			scopy = NULL;
		}
		if (scopy)
		{
			/* TODO: proper endianness detection, for now it's CPU endianness only */
			gst_structure_set(scopy, "signed", G_TYPE_BOOLEAN, TRUE, NULL);
			gst_structure_set(scopy, "endianness", G_TYPE_INT, G_BYTE_ORDER,
					NULL);
			gst_caps_append_structure(caps, scopy);
		}

	}

	avsys_pcm_format_mask_free(mask);
	gst_caps_unref(in_caps);
	return caps;
}


static GstCaps *
gst_avsysaudiosrc_detect_channels(GstObject * obj,
		avsys_pcm_hw_params_t * hw_params, GstCaps * in_caps)
{
	GstCaps *caps;
	guint min, max;
	gint err, min_channel, max_channel, i;
//	gint dir;

	GST_LOG_OBJECT(obj, "probing sample rates ...");

	if ((err = avsys_pcm_hw_params_get_channels_min(hw_params, &min)) < 0)
		goto min_chan_err;

	if ((err = avsys_pcm_hw_params_get_channels_max(hw_params, &max)) < 0)
		goto max_chan_err;

	min_channel = min;
	max_channel = max;

	if (min_channel < _MIN_CHANNEL)
		min_channel = _MIN_CHANNEL; /* random 'sensible minimum' */

	if (max_channel <= 0)
		max_channel = _MAX_CHANNEL; /* or maybe just use 192400 or so? */
	else if (max_channel > 0 && max_channel < _MIN_CHANNEL)
		max_channel = MAX (_MAX_CHANNEL, min_channel);

	GST_DEBUG_OBJECT(obj, "Min. channel = %u (%d)", min_channel, min);
	GST_DEBUG_OBJECT(obj, "Max. channel = %u (%d)", max_channel, max);

	caps = gst_caps_make_writable(in_caps);

	for (i = 0; i < gst_caps_get_size(caps); ++i)
	{
		GstStructure *s;

		s = gst_caps_get_structure(caps, i);
		if (min_channel == max_channel)
		{
			gst_structure_set(s, "channels", G_TYPE_INT, _MIN_CHANNEL, NULL);
		}
		else
		{
			gst_structure_set(s, "channels", GST_TYPE_INT_RANGE, min_channel,
					max_channel, NULL);
		}
	}

	return caps;

	/* ERRORS */
	min_chan_err:
	{
		GST_ERROR_OBJECT(obj, "failed to query minimum sample rate");
		gst_caps_unref(in_caps);
		return NULL;
	}
	max_chan_err:
	{
		GST_ERROR_OBJECT(obj, "failed to query maximum sample rate:");
		gst_caps_unref(in_caps);
		return NULL;
	}
}

/*
 * gst_avsys_probe_supported_formats:
 *
 * Takes the template caps and returns the subset which is actually
 * supported by this device.
 *
 */

static GstCaps *
gst_avsysaudiosrc_probe_supported_formats(GstObject * obj,
		avsys_handle_t handle, const GstCaps * template_caps)
{

	avsys_pcm_hw_params_t *hw_params;
	GstCaps *caps;
	gint err;

	avsys_pcm_hw_params_malloc(&hw_params);
	if ((err = avsys_pcm_hw_params_any(handle, hw_params)) < 0)
		goto error;

	caps = gst_caps_copy(template_caps);

	if (!(caps = gst_avsysaudiosrc_detect_formats(obj, hw_params, caps)))
		goto subroutine_error;

	if (!(caps = gst_avsysaudiosrc_detect_rates(obj, hw_params, caps)))
		goto subroutine_error;

	if (!(caps = gst_avsysaudiosrc_detect_channels(obj, hw_params, caps)))
		goto subroutine_error;

	avsys_pcm_hw_params_free(hw_params);
	return caps;

	/* ERRORS */
	error:
	{
		GST_ERROR_OBJECT(obj, "failed to query formats");
		return NULL;
	}
	subroutine_error:
	{
		GST_ERROR_OBJECT(obj, "failed to query formats");
		return NULL;
	}
}
#endif
