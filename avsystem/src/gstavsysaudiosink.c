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



#include <gst/gst.h>
#include <gst/gstutils.h>

#include <string.h>

#include "gstavsysaudiosink.h"

#define _ALSA_DAPM_
#define __REPLACE_RESET_WITH_CLOSE_AND_REOPEN__

#define CONVERT_MUTE_VALUE(_mute) ((_mute) ? AVSYS_AUDIO_MUTE : AVSYS_AUDIO_UNMUTE)

GST_DEBUG_CATEGORY_EXTERN (avsystem_sink_debug);
#define GST_CAT_DEFAULT avsystem_sink_debug

#define DEFAULT_USER_ROUTE 	AVSYSAUDIOSINK_USERROUTE_AUTO
#define DEFAULT_AUDIO_ROUTE 	AVSYSAUDIOSINK_AUDIOROUTE_USE_EXTERNAL_SETTING
#define DEFAULT_VOLUME_TYPE		AVSYS_AUDIO_VOLUME_TYPE_MEDIA
#define DEFAULT_MEDIACALL_MODE	AVSYS_AUDIO_ECHO_MODE_NONE
#define DEFAULT_FADEUP_VOLUME	FALSE
#define DEFAULT_AUDIO_MUTE	AVSYSAUDIOSINK_AUDIO_UNMUTE
#define DEFAULT_AUDIO_LATENCY	AVSYSAUDIOSINK_LATENCY_MID
#define DEFAULT_AUDIO_CLOSE_HANDLE_ON_PREPARE	FALSE

//GST_DEBUG_CATEGORY_STATIC (gst_avsystemsink_debug);

/* element factory information */
static const GstElementDetails gst_avsysaudiosink_details =
	GST_ELEMENT_DETAILS ("AV-system Audio OUT",
						"Sink/Audio",
						"Output to AV System",
						"Samsung Electronics co., ltd");

enum
{
	PROP_0,
	PROP_AUDIO_MUTE,
	PROP_AUDIO_VOLUME_TYPE,
	PROP_AUDIO_PRIORITY,
	PROP_AUDIO_FADEUPVOLUME,
	PROP_AUDIO_ROUTE_POLICY,
	PROP_AUDIO_USER_ROUTE,
	PROP_AUDIO_LATENCY,
	PROP_AUDIO_HANDLE,
	PROP_AUDIO_CALLBACK,
	PROP_AUDIO_CLOSE_HANDLE_ON_PREPARE,
};

GType
gst_avsysaudiosink_audio_mute_get_type (void)
{
	static GType avaudio_mute_type = 0;
	static const GEnumValue avaudio_mute[] = {
		{AVSYSAUDIOSINK_AUDIO_UNMUTE, "Unmute", "unmute"},
		{AVSYSAUDIOSINK_AUDIO_MUTE, "Mute immediately", "mute"},
		{AVSYSAUDIOSINK_AUDIO_MUTE_WITH_FADEDOWN_EFFECT, "Mute with fadedown effect", "fadedown"},
		{0, NULL, NULL},
	};

	if (!avaudio_mute_type) {
		avaudio_mute_type =
			g_enum_register_static ("GstAvsysAudioSinkAudioMute", avaudio_mute);
	}
	return avaudio_mute_type;
}

GType
gst_avsysaudiosink_user_route_get_type (void)
{
	static GType user_route_type = 0;
	static const GEnumValue user_route[] = {
		{AVSYSAUDIOSINK_USERROUTE_AUTO, "Route automatically", "auto"},
		{AVSYSAUDIOSINK_USERROUTE_PHONE, "Route to phone only", "phone"},
		{0, NULL, NULL},
	};

	if (!user_route_type) {
		user_route_type =
			g_enum_register_static ("GstAvsysAudioSinkUserRoutePolicy",user_route);
	}
	return user_route_type;
}

GType
gst_avsysaudiosink_audio_route_get_type (void)
{
	static GType playback_audio_route_type = 0;
	static const GEnumValue playback_audio_route[] = {
		{AVSYSAUDIOSINK_AUDIOROUTE_USE_EXTERNAL_SETTING, "Use external sound path", "external"},
		{AVSYSAUDIOSINK_AUDIOROUTE_PLAYBACK_NORMAL, "Auto change between speaker & earphone", "normal"},
		{AVSYSAUDIOSINK_AUDIOROUTE_PLAYBACK_ALERT, "Play via both speaker & earphone", "alert"},
		{AVSYSAUDIOSINK_AUDIOROUTE_PLAYBACK_HEADSET_ONLY, "Play via earphone only", "headset"},
		{0, NULL, NULL},
	};

	if (!playback_audio_route_type) {
		playback_audio_route_type =
			g_enum_register_static ("GstAvsysAudioSinkAudioRoutePolicy", playback_audio_route);
	}
	return playback_audio_route_type;
}

GType
gst_avsysaudiosink_latency_get_type (void)
{
	static GType avsysaudio_latency_type = 0;
	static const GEnumValue avsysaudio_latency[] = {
		{AVSYSAUDIOSINK_LATENCY_LOW, "Low latency", "low"},
		{AVSYSAUDIOSINK_LATENCY_MID, "Mid latency", "mid"},
		{AVSYSAUDIOSINK_LATENCY_HIGH, "High latency", "high"},
		{0, NULL, NULL},
	};

	if (!avsysaudio_latency_type) {
		avsysaudio_latency_type =
			g_enum_register_static ("GstAvsysAudioSinkLatency", avsysaudio_latency);
	}
	return avsysaudio_latency_type;
}

static void gst_avsysaudiosink_init_interfaces (GType type);

//#define GST_BOILERPLATE_FULL(type, type_as_function, parent_type, parent_type_macro, additional_initializations)

GST_BOILERPLATE_FULL (GstAvsysAudioSink, gst_avsysaudiosink, GstAudioSink,
					  GST_TYPE_AUDIO_SINK, gst_avsysaudiosink_init_interfaces);


static void gst_avsysaudiosink_finalise (GObject * object);
static void gst_avsysaudiosink_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_avsysaudiosink_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
#if 0	/*not use*/
static GstCaps *gst_avsysaudiosink_getcaps (GstBaseSink * bsink);
#endif

static gboolean gst_avsysaudiosink_avsys_close(GstAvsysAudioSink *avsys_audio);
static gboolean gst_avsysaudiosink_avsys_open(GstAvsysAudioSink *avsys_audio);

static gboolean gst_avsysaudiosink_open (GstAudioSink * asink);
static gboolean gst_avsysaudiosink_prepare (GstAudioSink * asink, GstRingBufferSpec * spec);
static gboolean gst_avsysaudiosink_unprepare (GstAudioSink * asink);
static gboolean gst_avsysaudiosink_close (GstAudioSink * asink);
static guint gst_avsysaudiosink_write (GstAudioSink * asink, gpointer data, guint length);
static guint gst_avsysaudiosink_delay (GstAudioSink * asink);
static void gst_avsysaudiosink_reset (GstAudioSink * asink);
static gboolean avsysaudiosink_post_message(GstAvsysAudioSink* self,int errorcode);


#define AVSYS_AUDIO_FACTORY_ENDIANNESS	"LITTLE_ENDIAN"


static GstStaticPadTemplate avsysaudiosink_sink_factory =
	GST_STATIC_PAD_TEMPLATE ("sink",
							 GST_PAD_SINK,
							 GST_PAD_ALWAYS,
							 GST_STATIC_CAPS ("audio/x-raw-int, "
									 "endianness = (int) { " AVSYS_AUDIO_FACTORY_ENDIANNESS " }, "
									 "signed = (boolean) { TRUE }, "
									 "width = (int) 16, "
									 "depth = (int) 16, "
									 "rate = (int) [ 1, MAX ], "
									 "channels = (int) [ 1, 6 ]; "
									 "audio/x-raw-int, "
									 "signed = (boolean) { FALSE }, "
									 "width = (int) 8, "
									 "depth = (int) 8, "
									 "rate = (int) [ 1, MAX ], "
									 "channels = (int) [ 1, 6 ] "
								 )
							 );
/*
static inline guint _time_to_sample(GstAvsysAudioSink * asink,  GstClockTime diff)
{
	guint result = 0;
	result =(GST_TIME_AS_USECONDS(diff) * asink->audio_param.samplerate)/1000000;
	return result;
}
*/

static void
gst_avsysaudiosink_finalise (GObject * object)
{
	GstAvsysAudioSink *sink = NULL;

	sink = GST_AVSYS_AUDIO_SINK (object);
	gst_avsysaudiosink_avsys_close(sink);
	g_mutex_free (sink->avsys_audio_lock);
	g_mutex_free (sink->avsys_audio_reset_lock);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_avsysaudiosink_init_interfaces (GType type)
{
	/* None */
}

static void
gst_avsysaudiosink_base_init (gpointer g_class)
{
	GstElementClass *element_class = NULL;

	element_class = GST_ELEMENT_CLASS (g_class);
	gst_element_class_set_details (element_class, &gst_avsysaudiosink_details);
	gst_element_class_add_pad_template (element_class, gst_static_pad_template_get (&avsysaudiosink_sink_factory));
}

static GstStateChangeReturn
gst_avsyssudiosink_change_state (GstElement *element, GstStateChange transition);


static void
gst_avsysaudiosink_class_init (GstAvsysAudioSinkClass * klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;
	GstBaseSinkClass *gstbasesink_class;
	GstBaseAudioSinkClass *gstbaseaudiosink_class;
	GstAudioSinkClass *gstaudiosink_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;
	gstbasesink_class = (GstBaseSinkClass *) klass;
	gstbaseaudiosink_class = (GstBaseAudioSinkClass *) klass;
	gstaudiosink_class = (GstAudioSinkClass *) klass;

	parent_class = g_type_class_peek_parent (klass);
	gstelement_class->change_state  = GST_DEBUG_FUNCPTR(gst_avsyssudiosink_change_state);

	gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_avsysaudiosink_finalise);
	gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_avsysaudiosink_get_property);
	gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_avsysaudiosink_set_property);

	//	gstbasesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_avsysaudiosink_getcaps);

	gstaudiosink_class->open = GST_DEBUG_FUNCPTR (gst_avsysaudiosink_open);
	gstaudiosink_class->prepare = GST_DEBUG_FUNCPTR (gst_avsysaudiosink_prepare);
	gstaudiosink_class->unprepare = GST_DEBUG_FUNCPTR (gst_avsysaudiosink_unprepare);
	gstaudiosink_class->close = GST_DEBUG_FUNCPTR (gst_avsysaudiosink_close);
	gstaudiosink_class->write = GST_DEBUG_FUNCPTR (gst_avsysaudiosink_write);
	gstaudiosink_class->delay = GST_DEBUG_FUNCPTR (gst_avsysaudiosink_delay);
	gstaudiosink_class->reset = GST_DEBUG_FUNCPTR (gst_avsysaudiosink_reset);

	g_object_class_install_property (  gobject_class, PROP_AUDIO_VOLUME_TYPE,
			g_param_spec_int ("volumetype", "Avsystem Volume Type",
					"Select avsystem audio software volume type", 0, G_MAXINT,
					DEFAULT_VOLUME_TYPE, G_PARAM_READWRITE));

	g_object_class_install_property (  gobject_class, PROP_AUDIO_PRIORITY,
			g_param_spec_int ("priority", "Avsystem Sound Priority", "Avsystem sound priority",
					AVSYS_AUDIO_PRIORITY_NORMAL, AVSYS_AUDIO_PRIORITY_SOLO_WITH_TRANSITION_EFFECT,
					AVSYS_AUDIO_PRIORITY_NORMAL, G_PARAM_READWRITE));

	g_object_class_install_property (  gobject_class, PROP_AUDIO_HANDLE,
			g_param_spec_pointer("audio-handle", "Avsystem handle",
					"Avsystem audio handle",
					G_PARAM_READWRITE));

	g_object_class_install_property (  gobject_class, PROP_AUDIO_CALLBACK,
			g_param_spec_pointer("audio-callback", "Avsystem callback",
					"Avsystem audio callback",
					G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_AUDIO_FADEUPVOLUME,
			g_param_spec_boolean ("fadeup", "Avsystem fadeup volume",
					"Enable avsystem audio fadeup volume when pause to play",
					DEFAULT_FADEUP_VOLUME, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class ,PROP_AUDIO_MUTE,
			g_param_spec_enum("mute", "Avsystem mute",
					"Avsystem audio mute",
					GST_AVSYS_AUDIO_SINK_MUTE, DEFAULT_AUDIO_MUTE,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS ));

	g_object_class_install_property (gobject_class ,PROP_AUDIO_ROUTE_POLICY,
			g_param_spec_enum("audio-route", "Audio Route Policy",
					"Audio route policy of system",
					GST_AVSYS_AUDIO_SINK_AUDIO_ROUTE, DEFAULT_AUDIO_ROUTE,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS ));

	g_object_class_install_property (gobject_class ,PROP_AUDIO_USER_ROUTE,
			g_param_spec_enum("user-route", "User Route Policy",
					"User route policy",
					GST_AVSYS_AUDIO_SINK_USER_ROUTE, DEFAULT_USER_ROUTE,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS ));

	g_object_class_install_property (gobject_class ,PROP_AUDIO_LATENCY,
			g_param_spec_enum("latency", "Audio Backend Latency",
					"Audio backend latency",
					GST_AVSYS_AUDIO_SINK_LATENCY_TYPE, DEFAULT_AUDIO_LATENCY,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS ));

	g_object_class_install_property (gobject_class, PROP_AUDIO_CLOSE_HANDLE_ON_PREPARE,
			g_param_spec_boolean ("close-handle-on-prepare", "Close Handle on Prepare",
					"Close Handle on Prepare",
					DEFAULT_AUDIO_CLOSE_HANDLE_ON_PREPARE, G_PARAM_READWRITE));
}

static void
gst_avsysaudiosink_set_property (GObject * object, guint prop_id,
							 const GValue * value, GParamSpec * pspec)
{
	GstAvsysAudioSink *sink = NULL;
	int nvalue = 0;
	gboolean nbool = FALSE;

	sink = GST_AVSYS_AUDIO_SINK (object);

	switch (prop_id) {
	case PROP_AUDIO_HANDLE:
		sink->cbHandle = g_value_get_pointer(value);
		break;
	case PROP_AUDIO_CALLBACK:
		sink->audio_stream_cb = g_value_get_pointer(value);
		break;
	case PROP_AUDIO_VOLUME_TYPE:
		nvalue = g_value_get_int(value);
		sink->volume_type = nvalue;
		if (sink->audio_handle != (avsys_handle_t)-1) {
			avsys_audio_update_volume_config(sink->audio_handle, sink->volume_type);
		}
		break;
	case PROP_AUDIO_PRIORITY:
		nvalue = g_value_get_int(value);
		sink->sound_priority = nvalue;
		break;
	case PROP_AUDIO_MUTE:
		nvalue = g_value_get_enum(value);
		if (sink->audio_handle != (avsys_handle_t)-1) {
			if(AVSYS_SUCCESS(avsys_audio_set_mute_fadedown(sink->audio_handle)))
				sink->mute = nvalue;
		} else {
			sink->mute = nvalue;
		}
		break;
	case PROP_AUDIO_FADEUPVOLUME:
		nbool = g_value_get_boolean(value);
		sink->use_fadeup_volume = nbool;
		break;
	case PROP_AUDIO_ROUTE_POLICY:
		nvalue = g_value_get_enum(value);
		sink->audio_route_policy = nvalue;
		switch (sink->audio_route_policy) {
		case AVSYSAUDIOSINK_AUDIOROUTE_USE_EXTERNAL_SETTING:
			GST_INFO_OBJECT(sink, "use external audio route setting");
			break;
		default:
			g_print("AVSYSAUDIOSINK :: Unknown audio route option %d\n", sink->audio_route_policy);
			GST_ERROR_OBJECT(sink, "Unknown audio route option %d", sink->audio_route_policy);
			break;
		}
		break;
	case PROP_AUDIO_USER_ROUTE:
		nvalue = g_value_get_enum(value);
		sink->user_route_policy = nvalue;
		break;
	case PROP_AUDIO_LATENCY:
		nvalue = g_value_get_enum(value);
		sink->latency = nvalue;
		break;
	case PROP_AUDIO_CLOSE_HANDLE_ON_PREPARE:
		nbool = g_value_get_boolean(value);
		sink->close_handle_on_prepare = nbool;
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gst_avsysaudiosink_get_property (GObject * object, guint prop_id,
							 GValue * value, GParamSpec * pspec)
{
	GstAvsysAudioSink *sink = NULL;

	sink = GST_AVSYS_AUDIO_SINK (object);

	switch (prop_id) {
	case PROP_AUDIO_VOLUME_TYPE:
		g_value_set_int(value, sink->volume_type);
		break;
	case PROP_AUDIO_PRIORITY:
		g_value_set_int(value, sink->sound_priority);
		break;
	case PROP_AUDIO_MUTE:
		g_value_set_enum(value, sink->mute);
		break;
	case PROP_AUDIO_FADEUPVOLUME:
		g_value_set_boolean(value, sink->use_fadeup_volume);
		break;
	case PROP_AUDIO_ROUTE_POLICY:
		g_value_set_enum(value, sink->audio_route_policy);
		break;
	case PROP_AUDIO_USER_ROUTE:
		g_value_set_enum(value, sink->user_route_policy);
		break;
	case PROP_AUDIO_LATENCY:
		g_value_set_enum(value, sink->latency);
		break;
	case PROP_AUDIO_CLOSE_HANDLE_ON_PREPARE:
		g_value_set_boolean(value, sink->close_handle_on_prepare);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gst_avsysaudiosink_init (GstAvsysAudioSink * avsysaudiosink, GstAvsysAudioSinkClass * g_class)
{
	GST_DEBUG_OBJECT (avsysaudiosink, "initializing avsysaudiosink");

	avsysaudiosink->audio_handle = (avsys_handle_t)-1;
	avsysaudiosink->cached_caps = NULL;
	avsysaudiosink->avsys_audio_lock = g_mutex_new ();
	avsysaudiosink->avsys_audio_reset_lock = g_mutex_new ();
	avsysaudiosink->volume_type = DEFAULT_VOLUME_TYPE;
	avsysaudiosink->sound_priority = AVSYS_AUDIO_PRIORITY_NORMAL;
	avsysaudiosink->mute = DEFAULT_AUDIO_MUTE;
	avsysaudiosink->use_fadeup_volume = DEFAULT_FADEUP_VOLUME;
	avsysaudiosink->latency = DEFAULT_AUDIO_LATENCY;
	avsysaudiosink->audio_route_policy = DEFAULT_AUDIO_ROUTE;
	avsysaudiosink->bytes_per_sample = 1;
	avsysaudiosink->close_handle_on_prepare = DEFAULT_AUDIO_CLOSE_HANDLE_ON_PREPARE;
#if defined (LPCM_DUMP_SUPPORT)
	avsysaudiosink->dumpFp = NULL;
#endif

}
#if 0
static GstCaps *
gst_avsysaudiosink_getcaps (GstBaseSink * bsink)
{
    GstElementClass *element_class = NULL;
    GstPadTemplate *pad_template = NULL;
    GstAvsysAudioSink *sink = GST_AVSYS_AUDIO_SINK (bsink);
    GstCaps *caps;

//    debug_fenter();

    sink = GST_AVSYS_AUDIO_SINK (bsink);
    if (sink->audio_handle == -1)
    {
        GST_DEBUG_OBJECT (sink, "avsystem audio not open, using template caps");
        return NULL;              /* base class will get template caps for us */
    }

    if (sink->cached_caps)
    {
        GST_LOG_OBJECT (sink, "Returning cached caps");
        return gst_caps_ref (sink->cached_caps);
    }

    element_class = GST_ELEMENT_GET_CLASS (sink);
    pad_template = gst_element_class_get_pad_template (element_class, "sink");
    g_return_val_if_fail (pad_template != NULL, NULL);

	// todo : get supported format.
	//caps = gst_avsysaudio_probe_supported_formats (GST_OBJECT (sink), sink->,
	//gst_pad_template_get_caps (pad_template));

	//if (caps) {
	//sink->cached_caps = gst_caps_ref (caps);
	//}

    GST_INFO_OBJECT (sink, "returning caps %" GST_PTR_FORMAT, caps);

    return caps;
}
#endif

static gboolean
avsysaudiosink_parse_spec (GstAvsysAudioSink * avsys_audio, GstRingBufferSpec * spec)
{
	/* Check param */
	if (spec->type != GST_BUFTYPE_LINEAR ||
		spec->channels > 6 || spec->channels < 1 ||
		!(spec->format == GST_S8 || spec->format == GST_S16_LE) )
		return FALSE;

	switch (spec->format) {
	case GST_S8:
		avsys_audio->audio_param.format = AVSYS_AUDIO_FORMAT_8BIT;
		avsys_audio->bytes_per_sample = 1;
		break;
	case GST_S16_LE:
		avsys_audio->audio_param.format = AVSYS_AUDIO_FORMAT_16BIT;
		avsys_audio->bytes_per_sample = 2;
		break;
	default:
		return FALSE;
	}

	/// set audio parameter for avsys audio open
	switch (avsys_audio->latency) {
	case AVSYSAUDIOSINK_LATENCY_LOW:
		avsys_audio->audio_param.mode = AVSYS_AUDIO_MODE_OUTPUT_VIDEO;
		break;
	case AVSYSAUDIOSINK_LATENCY_MID:
		avsys_audio->audio_param.mode = AVSYS_AUDIO_MODE_OUTPUT;
		break;
	case AVSYSAUDIOSINK_LATENCY_HIGH:
		avsys_audio->audio_param.mode = AVSYS_AUDIO_MODE_OUTPUT_CLOCK;
		break;
	}

	avsys_audio->audio_param.priority = 0;
	avsys_audio->audio_param.samplerate = spec->rate;
	avsys_audio->audio_param.channels = spec->channels;
	avsys_audio->bytes_per_sample *= spec->channels;

	/* set software volume table type */
	avsys_audio->audio_param.vol_type = avsys_audio->volume_type;
	avsys_audio->audio_param.priority = avsys_audio->sound_priority;
	avsys_audio->audio_param.handle_route = avsys_audio->user_route_policy;

	return TRUE;
}

static gboolean
gst_avsysaudiosink_open (GstAudioSink * asink)
{
	return TRUE;
}

static gboolean
gst_avsysaudiosink_prepare (GstAudioSink * asink, GstRingBufferSpec * spec)
{
	GstAvsysAudioSink *avsys_audio = NULL;
	guint	p_time = 0, b_time = 0;

	avsys_audio = GST_AVSYS_AUDIO_SINK (asink);

	GST_WARNING("Start");

	// set avsys audio param
	if (!avsysaudiosink_parse_spec (avsys_audio, spec))
		goto spec_parse;

	if (gst_avsysaudiosink_avsys_open(avsys_audio) == FALSE) {
		GST_ERROR_OBJECT(avsys_audio, "gst_avsysaudiosink_avsys_open() failed");
		goto OPEN_FAILED;
	}

	/* Ring buffer size */
	if (AVSYS_STATE_SUCCESS ==
		avsys_audio_get_period_buffer_time(avsys_audio->audio_handle, &p_time, &b_time)) {
		if(p_time == 0 || b_time == 0)
			return FALSE;

		spec->latency_time = (guint64)p_time;
		spec->buffer_time = (guint64)b_time;
	} else {
		GST_WARNING_OBJECT(avsys_audio, "");
		return FALSE;
	}
	spec->segsize = avsys_audio->avsys_size; /* '/16' see avsys_audio_open */
	spec->segtotal = (b_time / p_time) + (((b_time % p_time)/p_time > 0.5) ? 1: 0);
	//spec->segtotal+2;

	if (avsys_audio->close_handle_on_prepare) {
		if (gst_avsysaudiosink_avsys_close(avsys_audio) == FALSE) {
			GST_ERROR_OBJECT(avsys_audio, "gst_avsysaudiosink_avsys_close() failed");
			return FALSE;
		}
	}

	GST_WARNING("End");
	GST_WARNING_OBJECT (avsys_audio, "latency time %u, buffer time %u, seg total %u\n",
			(unsigned int)(spec->latency_time/1000), (unsigned int)(spec->buffer_time/1000), spec->segtotal);
	return TRUE;

spec_parse:
	{
		GST_ELEMENT_ERROR (avsys_audio, RESOURCE, SETTINGS, (NULL),
						   ("Setting of swparams failed: " ));
		return FALSE;
	}

OPEN_FAILED:
	avsysaudiosink_post_message(avsys_audio, GST_RESOURCE_ERROR_OPEN_READ);
	return FALSE;
}

static gboolean
gst_avsysaudiosink_unprepare (GstAudioSink * asink)
{
	GstAvsysAudioSink	*avsys_audio = NULL;
	gboolean			result = TRUE;
	avsys_audio = GST_AVSYS_AUDIO_SINK (asink);

	GST_WARNING("Start");
	if(!gst_avsysaudiosink_avsys_close(avsys_audio))
	{
		GST_ERROR_OBJECT(avsys_audio, "gst_avsysaudiosink_avsys_close() failed");
		result = FALSE;
	}
	GST_WARNING("End");

	return result;
}

static gboolean
gst_avsysaudiosink_close (GstAudioSink * asink)
{
	GstAvsysAudioSink *avsys_audio = NULL;

	avsys_audio = GST_AVSYS_AUDIO_SINK (asink);
	gst_caps_replace (&avsys_audio->cached_caps, NULL);

	return TRUE;
}


/*
 *   Underrun and suspend recovery
 */

static guint
gst_avsysaudiosink_write (GstAudioSink * asink, gpointer data, guint length)
{
	GstAvsysAudioSink *avsys_audio = NULL;
	gint	write_len = 0;

	avsys_audio = GST_AVSYS_AUDIO_SINK (asink);
	GST_AVSYS_AUDIO_SINK_LOCK (asink);

	if (avsys_audio->audio_stream_cb == NULL) {
	write_len = avsys_audio_write(avsys_audio->audio_handle, data, length);

#if defined (LPCM_DUMP_SUPPORT)
		fwrite(data, 1, write_len, avsys_audio->dumpFp); //This is for original data (no volume convert)
#endif
		if(write_len != length)
		{
		    goto write_error;
		}

		GST_AVSYS_AUDIO_SINK_UNLOCK (asink);

		return write_len;
write_error:
		{
			GST_AVSYS_AUDIO_SINK_UNLOCK (asink);

			if(AVSYS_FAIL(write_len))
			{
				GST_ERROR_OBJECT(avsys_audio, "avsys_audio_write() failed with %d\n", write_len);
			}
			return length;              /* skip one period */
		}
	} else {
		gboolean result;
		result = avsys_audio->audio_stream_cb(data, length, avsys_audio->cbHandle);
		if(!result)
		{
			GST_ERROR_OBJECT(avsys_audio,"auido stream callback failed\n");
		}
		GST_AVSYS_AUDIO_SINK_UNLOCK (asink);
		return length;
	}
}

static guint
gst_avsysaudiosink_delay (GstAudioSink * asink)
{
	GstAvsysAudioSink *avsys_audio = NULL;
	int delay = 0;
	guint retValue = 0;

	avsys_audio = GST_AVSYS_AUDIO_SINK (asink);
	GST_AVSYS_AUDIO_SINK_RESET_LOCK (asink);
	if ((int)avsys_audio->audio_handle != -1) {
		if (AVSYS_STATE_SUCCESS == avsys_audio_delay(avsys_audio->audio_handle, &delay)) {
			retValue = delay;
		}
	}
	GST_AVSYS_AUDIO_SINK_RESET_UNLOCK (asink);
	return retValue;
}

static void
gst_avsysaudiosink_reset (GstAudioSink * asink)
{
	GstAvsysAudioSink *avsys_audio = NULL;
	int avsys_result = AVSYS_STATE_SUCCESS;

	GST_WARNING("Start");
	GST_AVSYS_AUDIO_SINK_LOCK (asink);
	avsys_audio = GST_AVSYS_AUDIO_SINK (asink);

#if defined(__REPLACE_RESET_WITH_CLOSE_AND_REOPEN__)
	GST_AVSYS_AUDIO_SINK_RESET_LOCK (asink);
	avsys_result = avsys_audio_close (avsys_audio->audio_handle);
	if(AVSYS_FAIL(avsys_result)) {
		GST_ERROR_OBJECT (avsys_audio, "avsys_audio_close: internal error: ");
	} else {
		avsys_audio->audio_handle =(avsys_handle_t) -1;
	}
	GST_AVSYS_AUDIO_SINK_RESET_UNLOCK (asink);
#else
	if(AVSYS_STATE_SUCCESS != avsys_audio_reset(avsys_audio->audio_handle)) {
		GST_ERROR_OBJECT (avsys_audio, "avsys-reset: internal error: ");
	}
#endif

	GST_AVSYS_AUDIO_SINK_UNLOCK (asink);
	GST_WARNING("End");

	return;
}


static gboolean
gst_avsysaudiosink_avsys_open(GstAvsysAudioSink *avsys_audio)
{
	int avsys_result;

	GST_AVSYS_AUDIO_SINK_LOCK(avsys_audio);
	if (avsys_audio->audio_handle == (avsys_handle_t)-1) {

		GST_LOG_OBJECT (avsys_audio, "avsys_audio_open() with user policy [%d] ",  avsys_audio->user_route_policy);

		avsys_result = avsys_audio_open(&avsys_audio->audio_param, &avsys_audio->audio_handle, &avsys_audio->avsys_size);
		if (avsys_result != AVSYS_STATE_SUCCESS) {
			GST_AVSYS_AUDIO_SINK_UNLOCK (avsys_audio);
			avsysaudiosink_post_message(avsys_audio, GST_RESOURCE_ERROR_OPEN_READ);
			return FALSE;
		}

		GST_LOG_OBJECT (avsys_audio, "Opened av system ");

		GST_AVSYS_AUDIO_SINK_UNLOCK (avsys_audio);
	} else {
		GST_WARNING_OBJECT(avsys_audio, "audio handle has already opened");
		GST_AVSYS_AUDIO_SINK_UNLOCK (avsys_audio);
		return FALSE;
	}
#if defined (LPCM_DUMP_SUPPORT)
	if (avsys_audio->dumpFp == NULL) {
		avsys_audio->dumpFp = fopen("/root/dump.lpcm","w");
	}
#endif
	return TRUE;
}

static gboolean
gst_avsysaudiosink_avsys_close(GstAvsysAudioSink *avsys_audio)
{
	int avsys_result = AVSYS_STATE_SUCCESS;

	GST_AVSYS_AUDIO_SINK_LOCK (avsys_audio);

	if (avsys_audio->audio_handle !=  (avsys_handle_t)-1 ) {
		avsys_result = avsys_audio_close(avsys_audio->audio_handle);

		if (AVSYS_FAIL(avsys_result))
		{
			GST_ERROR_OBJECT(avsys_audio,
					"avsys_audio_close() failed with 0x%x", avsys_result);
			GST_AVSYS_AUDIO_SINK_UNLOCK (avsys_audio);
			return FALSE;
		} else {
			avsys_audio->audio_handle = (avsys_handle_t) -1;
			GST_INFO_OBJECT(avsys_audio, "avsys_audio_close() success");
		}

		GST_LOG_OBJECT (avsys_audio, "Closed av system ");
	} else {
		GST_WARNING_OBJECT(avsys_audio, "audio handle has already closed");
	}

	GST_AVSYS_AUDIO_SINK_UNLOCK (avsys_audio);
#if defined (LPCM_DUMP_SUPPORT)
	if(avsys_audio->dumpFp != NULL)
	{
		fclose(avsys_audio->dumpFp);
		avsys_audio->dumpFp = NULL;
	}
#endif
	return TRUE;
}

static GstStateChangeReturn
gst_avsyssudiosink_change_state (GstElement *element, GstStateChange transition)
{
	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
	GstAvsysAudioSink *avsys_audio = GST_AVSYS_AUDIO_SINK (element);

	int avsys_result = AVSYS_STATE_SUCCESS;

	switch (transition) {
		case GST_STATE_CHANGE_NULL_TO_READY:
			break;
		case GST_STATE_CHANGE_READY_TO_PAUSED:
			break;
		case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
#if defined(_ALSA_DAPM_)
			switch(avsys_audio->audio_route_policy) {
			case AVSYSAUDIOSINK_AUDIOROUTE_USE_EXTERNAL_SETTING:
				GST_INFO_OBJECT(avsys_audio, "audio route uses external setting");
				break;
			default:
				GST_ERROR_OBJECT(avsys_audio, "Unknown audio route option %d\n", avsys_audio->audio_route_policy);
				break;
			}
#endif
#if defined(__REPLACE_RESET_WITH_CLOSE_AND_REOPEN__)
			if (avsys_audio->audio_handle == (avsys_handle_t)-1) {
				avsys_result = avsys_audio_open(&avsys_audio->audio_param, &avsys_audio->audio_handle, &avsys_audio->avsys_size);
				if (AVSYS_FAIL(avsys_result)) {
					GST_ERROR_OBJECT (avsys_audio, "avsys_audio_open: internal error: ");
					return GST_STATE_CHANGE_FAILURE;
				}
			}
#endif

			if (avsys_audio->use_fadeup_volume) {
				GST_INFO_OBJECT(avsys_audio, "Set fadeup volume");
				avsys_audio_set_volume_fadeup(avsys_audio->audio_handle);
			}

			if(AVSYS_STATE_SUCCESS != avsys_audio_set_mute(avsys_audio->audio_handle, CONVERT_MUTE_VALUE(avsys_audio->mute)))
			{
				GST_ERROR_OBJECT(avsys_audio, "Set mute failed %d", CONVERT_MUTE_VALUE(avsys_audio->mute));
			}
			break;
		default:
			break;
	}
	ret = GST_ELEMENT_CLASS(parent_class)->change_state (element, transition);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		return ret;
	}
	switch (transition) {
	case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
		break;
	case GST_STATE_CHANGE_PAUSED_TO_READY:
		break;
	case GST_STATE_CHANGE_READY_TO_NULL:
		break;
	default:
		break;
	}
	return ret;
}


static gboolean
avsysaudiosink_post_message(GstAvsysAudioSink* self,int errorcode)
{
		GST_DEBUG("avsysaudiosink_post_message\n");
		gboolean ret = TRUE;
		GstMessage *Msg = NULL;
		GQuark domain;
		gboolean status = FALSE;
		GError *error = NULL;
		gint error_code;
		/*
		if(errorcode>0)
				error_code = errorcode;
		else
				error_code = GST_STREAM_ERROR_TYPE_NOT_FOUND; */
		error_code = errorcode;
		domain = gst_resource_error_quark();
		error = g_error_new (domain, error_code, "AVSYSAUDIOSINK_RESOURCE_ERROR");
		Msg = gst_message_new_error(GST_ELEMENT(self), error, "AVSYSAUDIOSINK_ERROR");
		status = gst_element_post_message (GST_ELEMENT(self), Msg);
		if (status == FALSE)
		{
				GST_ERROR("Error in posting message on the bus ...\n");
				ret = FALSE;
		}

		return ret;
}
