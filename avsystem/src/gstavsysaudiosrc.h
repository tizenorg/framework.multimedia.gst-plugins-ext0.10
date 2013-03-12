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

#ifndef __GST_AVSYS_AUDIO_SRC_H__
#define __GST_AVSYS_AUDIO_SRC_H__

//#undef  _MMCAMCORDER_MERGE_TEMP 

#include <gst/audio/gstaudiosrc.h>
#include <glib.h>

#include <avsys-audio.h>

G_BEGIN_DECLS

#define GST_TYPE_AVSYS_AUDIO_SRC            (gst_avsysaudiosrc_get_type())
#define GST_AVSYS_AUDIO_SRC(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVSYS_AUDIO_SRC,GstAvsysAudioSrc))
#define GST_AVSYS_AUDIO_SRC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVSYS_AUDIO_SRC,GstAvsysAudioSrcClass))
#define GST_IS_AVSYS_AUDIO_SRC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVSYS_AUDIO_SRC))
#define GST_IS_AVSYS_AUDIO_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVSYS_AUDIO_SRC))
#define GST_AVSYS_AUDIO_SRC_CAST(obj)       ((GstAvsysAudioSrc *)(obj))

#define GST_AVSYS_AUDIO_SRC_GET_LOCK(obj)  (GST_AVSYS_AUDIO_SRC_CAST (obj)->avsysaudio_lock)
#define GST_AVSYS_AUDIO_SRC_LOCK(obj)      (g_mutex_lock (GST_AVSYS_AUDIO_SRC_GET_LOCK (obj)))
#define GST_AVSYS_AUDIO_SRC_UNLOCK(obj)    (g_mutex_unlock (GST_AVSYS_AUDIO_SRC_GET_LOCK (obj)))

typedef struct _GstAvsysAudioSrc GstAvsysAudioSrc;
typedef struct _GstAvsysAudioSrcClass GstAvsysAudioSrcClass;

typedef enum {
	AVSYSAUDIOSRC_LATENCY_LOW = 0,
	AVSYSAUDIOSRC_LATENCY_MID,
	AVSYSAUDIOSRC_LATENCY_HIGH,
}GstAvsysAudioSrcAudioLatency;

#define GST_AVSYS_AUDIO_SRC_LATENCY	(gst_avsysaudiosrc_audio_latency_get_type())



/**
 * GstAvsysAudioSrc:
 *
 * Opaque data structure
 */
struct _GstAvsysAudioSrc {
	GstAudioSrc					src;

	avsys_handle_t				audio_handle;
	avsys_audio_param_t 		audio_param;

	gint						buffer_size;
	GstCaps						*cached_caps;
	GMutex						*avsysaudio_lock;

	gint						media_call;
	gint						bytes_per_sample;
	gint						latency;

};

struct _GstAvsysAudioSrcClass {
	GstAudioSrcClass parent_class;
};


GType gst_avsysaudiosrc_get_type (void);
GType gst_avsysaudiosrc_audio_latency_get_type(void);

G_END_DECLS

#endif /* __GST_AVSYS_AUDIO_SRC_H__ */
