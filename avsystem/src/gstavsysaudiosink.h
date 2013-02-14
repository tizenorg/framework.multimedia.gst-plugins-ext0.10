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


#ifndef __GST_AVSYSAUDIOSINK_H__
#define __GST_AVSYSAUDIOSINK_H__

#include <gst/gst.h>
#include <gst/audio/gstaudiosink.h>

#include <avsys-audio.h>

G_BEGIN_DECLS

#define GST_TYPE_AVSYS_AUDIO_SINK             (gst_avsysaudiosink_get_type())
#define GST_AVSYS_AUDIO_SINK(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVSYS_AUDIO_SINK,GstAvsysAudioSink))
#define GST_AVSYS_AUDIO_SINK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVSYS_AUDIO_SINK,GstAvsysAudioSinkClass))
#define GST_IS_AVSYS_AUDIO_SINK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVSYS_AUDIO_SINK))
#define GST_IS_AVSYS_AUDIO_SINK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVSYS_AUDIO_SINK))
//#define GST_AVSYS_AUDIO_SINK_CAST(obj)        ((GstAvsysAudioSink *) (obj))

typedef struct _GstAvsysAudioSink      GstAvsysAudioSink;
typedef struct _GstAvsysAudioSinkClass GstAvsysAudioSinkClass;

#define GST_AVSYS_AUDIO_SINK_GET_LOCK(obj) (GST_AVSYS_AUDIO_SINK (obj)->avsys_audio_lock)
#define GST_AVSYS_AUDIO_SINK_LOCK(obj)     (g_mutex_lock (GST_AVSYS_AUDIO_SINK_GET_LOCK(obj)))
#define GST_AVSYS_AUDIO_SINK_UNLOCK(obj)   (g_mutex_unlock (GST_AVSYS_AUDIO_SINK_GET_LOCK(obj)))

#define GST_AVSYS_AUDIO_SINK_GET_RESET_LOCK(obj) (GST_AVSYS_AUDIO_SINK (obj)->avsys_audio_reset_lock)
#define GST_AVSYS_AUDIO_SINK_RESET_LOCK(obj)     (g_mutex_lock (GST_AVSYS_AUDIO_SINK_GET_RESET_LOCK(obj)))
#define GST_AVSYS_AUDIO_SINK_RESET_UNLOCK(obj)   (g_mutex_unlock (GST_AVSYS_AUDIO_SINK_GET_RESET_LOCK(obj)))


typedef enum  {
	AVSYSAUDIOSINK_AUDIO_UNMUTE = 0,
	AVSYSAUDIOSINK_AUDIO_MUTE,
	AVSYSAUDIOSINK_AUDIO_MUTE_WITH_FADEDOWN_EFFECT,
}GstAvsysAudioSinkAudioMute;


typedef enum  {
	AVSYSAUDIOSINK_AUDIOROUTE_USE_EXTERNAL_SETTING = -1,
	AVSYSAUDIOSINK_AUDIOROUTE_PLAYBACK_NORMAL,
	AVSYSAUDIOSINK_AUDIOROUTE_PLAYBACK_ALERT,
	AVSYSAUDIOSINK_AUDIOROUTE_PLAYBACK_HEADSET_ONLY
}GstAvsysAudioSinkAudioRoutePolicy;


typedef enum  {
	AVSYSAUDIOSINK_USERROUTE_AUTO = 0,
 	AVSYSAUDIOSINK_USERROUTE_PHONE	
}GstAvsysAudioSinkUserRoutePolicy;

typedef enum {
	AVSYSAUDIOSINK_LATENCY_LOW = 0,
	AVSYSAUDIOSINK_LATENCY_MID,
	AVSYSAUDIOSINK_LATENCY_HIGH,
}GstAvsysAudioSinkLatency;

#define GST_AVSYS_AUDIO_SINK_USER_ROUTE 		(gst_avsysaudiosink_user_route_get_type ())
#define GST_AVSYS_AUDIO_SINK_AUDIO_ROUTE 	(gst_avsysaudiosink_audio_route_get_type ())
#define GST_AVSYS_AUDIO_SINK_MUTE			(gst_avsysaudiosink_audio_mute_get_type())
#define GST_AVSYS_AUDIO_SINK_LATENCY_TYPE	(gst_avsysaudiosink_latency_get_type ())

//this define if for debugging
//#define LPCM_DUMP_SUPPORT
struct _GstAvsysAudioSink {
    GstAudioSink				sink;

	avsys_handle_t				audio_handle;
	avsys_audio_param_t     	audio_param;
	gint                		avsys_size;
	gint						mute;
	gint						use_fadeup_volume;
	gint						audio_route_policy;
	gint						user_route_policy;
	gint						latency;

	GstCaps						*cached_caps;

	GMutex						*avsys_audio_lock;
	GMutex						*avsys_audio_reset_lock;
	gint						volume_type;
	gint						sound_priority;
	gint						bytes_per_sample;

	gpointer                         cbHandle;
	gboolean (*audio_stream_cb) (void *stream, int stream_size, void *user_param);
};


struct _GstAvsysAudioSinkClass {
	GstAudioSinkClass    parent_class;
};

GType gst_avsysaudiosink_get_type (void);
GType gst_avsysaudiosink_audio_route_get_type (void);
GType gst_avsysaudiosink_audio_mute_get_type(void);
GType gst_avsysaudiosink_latency_get_type (void);

G_END_DECLS

#endif /* __GST_AVSYSAUDIOSINK_H__ */
