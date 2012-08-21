/*
 * evasimagesink
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Sangchul Lee <sc11.lee@samsung.com>
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

/**
 * SECTION:element-evasimagesink
 * Gstreamer Evas Video Sink - draw video on the given Evas Image Object
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>
#include <Evas.h>
#include <Ecore.h>
#include <Ecore_X.h>

#include "gstevasimagesink.h"

#define CAP_WIDTH "width"
#define CAP_HEIGHT "height"

GST_DEBUG_CATEGORY_STATIC (gst_evas_image_sink_debug);
#define GST_CAT_DEFAULT gst_evas_image_sink_debug

/* Enumerations */
enum
{
	/* FILL ME */
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_EVAS_OBJECT,
	PROP_EVAS_OBJECT_SHOW,
};

#define COLOR_DEPTH 4
#define GL_X11_ENGINE "gl_x11"

GMutex *instance_lock;
guint instance_lock_count;

static inline gboolean
is_evas_image_object (Evas_Object *obj)
{
	const char *type;
	if (!obj) {
		return FALSE;
	}
	type = evas_object_type_get (obj);
	if (!type) {
		return FALSE;
	}
	if (strcmp (type, "image") == 0) {
		return TRUE;
	}
	return FALSE;
}

/* the capabilities of the inputs.
 *
 * BGRx format
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_BGRx));

GST_BOILERPLATE (GstEvasImageSink, gst_evas_image_sink, GstVideoSink, GST_TYPE_VIDEO_SINK);

static void gst_evas_image_sink_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gst_evas_image_sink_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static gboolean gst_evas_image_sink_set_caps (GstBaseSink *base_sink, GstCaps *caps);
static GstFlowReturn gst_evas_image_sink_show_frame (GstVideoSink *video_sink, GstBuffer *buf);
static gboolean gst_evas_image_sink_event (GstBaseSink *sink, GstEvent *event);
static GstStateChangeReturn gst_evas_image_sink_change_state (GstElement *element, GstStateChange transition);
static void evas_image_sink_cb_del_eo (void *data, Evas *e, Evas_Object *obj, void *event_info);
static void evas_image_sink_cb_resize_event (void *data, Evas *e, Evas_Object *obj, void *event_info);

static void
gst_evas_image_sink_base_init (gpointer gclass)
{
	GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

	gst_element_class_set_details_simple (element_class,
		"EvasImageSink",
		"VideoSink",
		"Video sink element for evas image object",
		"Wonguk Jeong <wonguk.jeong@samsung.com>");

	gst_element_class_add_pad_template (element_class,
	gst_static_pad_template_get (&sink_factory));
}

static void
gst_evas_image_sink_class_init (GstEvasImageSinkClass *klass)
{
	GObjectClass *gobject_class;
	GstBaseSinkClass *gstbasesink_class;
	GstVideoSinkClass *gstvideosink_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstbasesink_class = GST_BASE_SINK_CLASS (klass);
	gstvideosink_class = GST_VIDEO_SINK_CLASS (klass);
	gstelement_class = (GstElementClass *) klass;

	gobject_class->set_property = gst_evas_image_sink_set_property;
	gobject_class->get_property = gst_evas_image_sink_get_property;

	g_object_class_install_property (gobject_class, PROP_EVAS_OBJECT,
		g_param_spec_pointer ("evas-object", "Destination Evas Object",	"Destination evas image object", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_EVAS_OBJECT_SHOW,
		g_param_spec_boolean ("visible", "Show Evas Object", "When disabled, evas object does not show", TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	gstvideosink_class->show_frame = GST_DEBUG_FUNCPTR (gst_evas_image_sink_show_frame);
	gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_evas_image_sink_set_caps);
	gstbasesink_class->event = GST_DEBUG_FUNCPTR (gst_evas_image_sink_event);
	gstelement_class->change_state = GST_DEBUG_FUNCPTR(gst_evas_image_sink_change_state);
}

static void
gst_evas_image_sink_fini (gpointer data, GObject *obj)
{
	GST_INFO ("enter");

	GstEvasImageSink *esink = GST_EVASIMAGESINK (obj);
	if (!esink) {
		return;
	}
	if (esink->oldbuf) {
		gst_buffer_unref (esink->oldbuf);
	}

	g_mutex_lock (instance_lock);
	instance_lock_count--;
	g_mutex_unlock (instance_lock);
	if (instance_lock_count == 0) {
		g_mutex_free (instance_lock);
		instance_lock = NULL;
	}
}

static void
evas_image_sink_cb_pipe (void *data, void *buffer, unsigned int nbyte)
{
	GstBuffer *buf;
	GstEvasImageSink *esink = data;
	void *img_data;

	if (!data || !buffer) {
		return;
	}
	if (nbyte != sizeof (GstBuffer *)) {
		return;
	}
	if (!esink->eo) {
		return;
	}
	if (GST_STATE(esink) < GST_STATE_PAUSED) {
		GST_WARNING ("WRONG-STATE(%d) for rendering, skip this frame", GST_STATE(esink));
		return;
	}

	memcpy (&buf, buffer, sizeof (GstBuffer *));
	if (!buf) {
		GST_ERROR ("There is no buffer");
		return;
	}
	if (esink->present_data_addr == -1) {
		/* if present_data_addr is -1, we don't use this member variable */
	} else if (esink->present_data_addr != GST_BUFFER_DATA (buf)) {
		GST_WARNING ("skip rendering this buffer, present_data_addr:%x, GST_BUFFER_DATA(buf):%x", esink->present_data_addr,GST_BUFFER_DATA(buf));
		return;
	}

	MMTA_ACUM_ITEM_BEGIN("eavsimagesink _cb_pipe total", FALSE);

	if ( !esink->is_evas_object_size_set && esink->w > 0 && esink->h > 0) {
			evas_object_image_size_set (esink->eo, esink->w, esink->h);
			GST_DEBUG("evas_object_image_size_set(), width(%d),height(%d)",esink->w,esink->h);
			esink->is_evas_object_size_set = TRUE;
	}
	if (esink->gl_zerocopy) {
		img_data = evas_object_image_data_get (esink->eo, EINA_TRUE);
		if (!img_data || !GST_BUFFER_DATA(buf)) {
			GST_WARNING ("Cannot get image data from evas object or cannot get gstbuffer data");
			evas_object_image_data_set(esink->eo, img_data);
		} else {
			GST_DEBUG ("img_data(%x), GST_BUFFER_DATA(buf):%x, esink->w(%d),esink->h(%d)",img_data,GST_BUFFER_DATA(buf),esink->w,esink->h);
			__ta__("evasimagesink memcpy in _cb_pipe", memcpy (img_data, GST_BUFFER_DATA (buf), esink->w * esink->h * COLOR_DEPTH););
			evas_object_image_pixels_dirty_set (esink->eo, 1);
			evas_object_image_data_set(esink->eo, img_data);
		}
		gst_buffer_unref (buf);
	} else {
		GST_DEBUG ("GST_BUFFER_DATA(buf):%x",GST_BUFFER_DATA(buf));
		evas_object_image_data_set (esink->eo, GST_BUFFER_DATA (buf));
		evas_object_image_pixels_dirty_set (esink->eo, 1);
		if (esink->oldbuf) {
			gst_buffer_unref(esink->oldbuf);
		}
		esink->oldbuf = buf;
	}

	MMTA_ACUM_ITEM_END("eavsimagesink _cb_pipe total", FALSE);
}

static void
gst_evas_image_sink_init (GstEvasImageSink *esink, GstEvasImageSinkClass *gclass)
{
	GST_INFO ("enter");

	esink->eo = NULL;
	esink->epipe = NULL;
	esink->object_show = FALSE;
	esink->gl_zerocopy = FALSE;
	esink->is_evas_object_size_set = FALSE;
	esink->present_data_addr = -1;

	if(!instance_lock) {
		instance_lock = g_mutex_new();
	}
	g_mutex_lock (instance_lock);
	instance_lock_count++;
	g_mutex_unlock (instance_lock);

	g_object_weak_ref (G_OBJECT (esink), gst_evas_image_sink_fini, NULL);
}

static void
evas_image_sink_cb_del_eo (void *data, Evas *e, Evas_Object *obj, void *event_info)
{
	GstEvasImageSink *esink = data;
	if (!esink) {
		return;
	}

	evas_object_event_callback_del (esink->eo, EVAS_CALLBACK_RESIZE, evas_image_sink_cb_resize_event);
	if (esink->oldbuf) {
		gst_buffer_unref (esink->oldbuf);
		esink->oldbuf = NULL;
		esink->eo = NULL;
	}
}

static void
evas_image_sink_cb_resize_event (void *data, Evas *e, Evas_Object *obj, void *event_info)
{
	int w = 0;
	int h = 0;
	GstEvasImageSink *esink = data;
	if (!esink) {
		return;
	}

	evas_object_geometry_get(esink->eo, NULL, NULL, &w, &h);
	if (!w || !h) {
		GST_WARNING ("evas object size (w:%d,h:%d) was not set",w,h);
	} else {
		evas_object_image_fill_set(esink->eo, 0, 0, w, h);
		GST_DEBUG ("evas object fill set (w:%d,h:%d)",w,h);
	}
}

static int
evas_image_sink_get_size_from_caps (GstCaps *caps, int *w, int *h)
{
	gboolean r;
	int width, height;
	GstStructure *s;

	if (!caps || !w || !h) {
		return -1;
	}
	s = gst_caps_get_structure (caps, 0);
	if (!s) {
		return -1;
	}

	r = gst_structure_get_int (s, CAP_WIDTH, &width);
	if (r == FALSE) {
		return -1;
	}

	r = gst_structure_get_int (s, CAP_HEIGHT, &height);
	if (r == FALSE) {
		return -1;
	}

	*w = width;
	*h = height;
	return 0;
}

static gboolean
is_zerocopy_supported (Evas *e)
{
	Eina_List *engines, *l;
	int cur_id;
	int id;
	char *name;

	if (!e) {
		return FALSE;
	}

	engines = evas_render_method_list ();
	if (!engines) {
		return FALSE;
	}

	cur_id = evas_output_method_get (e);

	EINA_LIST_FOREACH (engines, l, name) {
		id = evas_render_method_lookup (name);
		if (name && id == cur_id) {
			if (!strcmp (name, GL_X11_ENGINE)) {
				return TRUE;
			}
			break;
		}
	}
	return FALSE;
}

static int
evas_image_sink_event_parse_data (GstEvasImageSink *esink, GstEvent *event)
{
	const GstStructure *st;
	guint st_data_addr = 0;
	gint st_data_width = 0;
	gint st_data_height = 0;

	g_return_val_if_fail (event != NULL, FALSE);
	g_return_val_if_fail (esink != NULL, FALSE);

	if (GST_EVENT_TYPE (event) != GST_EVENT_CUSTOM_DOWNSTREAM_OOB) {
		GST_WARNING ("it's not a custom downstream oob event");
		return -1;
	}
	st = gst_event_get_structure (event);
	if (st == NULL || !gst_structure_has_name (st, "GstStructureForCustomEvent")) {
		GST_WARNING ("structure in a given event is not proper");
		return -1;
	}
	if (!gst_structure_get_uint (st, "data-addr", &st_data_addr)) {
		GST_WARNING ("parsing data-addr failed");
		return -1;
	}
	esink->present_data_addr = st_data_addr;

	return 0;
}

static gboolean
gst_evas_image_sink_event (GstBaseSink *sink, GstEvent *event)
{
	GstEvasImageSink *esink = GST_EVASIMAGESINK (sink);
	GstMessage *msg;
	gchar *str;

	switch (GST_EVENT_TYPE (event)) {
		case GST_EVENT_FLUSH_START:
			GST_DEBUG ("GST_EVENT_FLUSH_START");
			break;
		case GST_EVENT_FLUSH_STOP:
			GST_DEBUG ("GST_EVENT_FLUSH_STOP");
			break;
		case GST_EVENT_EOS:
			GST_DEBUG ("GST_EVENT_EOS");
			break;
		case GST_EVENT_CUSTOM_DOWNSTREAM_OOB:
			if(!evas_image_sink_event_parse_data(esink, event)) {
				GST_DEBUG ("GST_EVENT_CUSTOM_DOWNSTREAM_OOB, present_data_addr:%x",esink->present_data_addr);
			} else {
				GST_ERROR ("evas_image_sink_event_parse_data() failed");
			}
			break;
		default:
			break;
	}
	if (GST_BASE_SINK_CLASS (parent_class)->event) {
		return GST_BASE_SINK_CLASS (parent_class)->event (sink, event);
	} else {
		return TRUE;
	}
}

static GstStateChangeReturn
gst_evas_image_sink_change_state (GstElement *element, GstStateChange transition)
{
	GstStateChangeReturn ret_state = GST_STATE_CHANGE_SUCCESS;
	GstEvasImageSink *esink = NULL;
	esink = GST_EVASIMAGESINK(element);
	int ret = 0;

	if(!esink) {
		GST_ERROR("can not get evasimagesink from element");
	}
	switch (transition) {
		case GST_STATE_CHANGE_NULL_TO_READY:
			GST_INFO ("*** STATE_CHANGE_NULL_TO_READY ***");
			break;
		case GST_STATE_CHANGE_READY_TO_PAUSED:
			GST_INFO ("*** STATE_CHANGE_READY_TO_PAUSED ***");
			break;
		case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
			GST_INFO ("*** STATE_CHANGE_PAUSED_TO_PLAYING ***");
			break;
		default:
			break;
	}

	ret_state = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

	switch (transition) {
		case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
			GST_INFO ("*** STATE_CHANGE_PLAYING_TO_PAUSED ***");
			break;
		case GST_STATE_CHANGE_PAUSED_TO_READY:
			GST_INFO ("*** STATE_CHANGE_PAUSED_TO_READY ***");
			break;
		case GST_STATE_CHANGE_READY_TO_NULL:
			GST_INFO ("*** STATE_CHANGE_READY_TO_NULL ***");
			evas_object_event_callback_del(esink->eo, EVAS_CALLBACK_DEL, evas_image_sink_cb_del_eo);
			evas_object_event_callback_del(esink->eo, EVAS_CALLBACK_RESIZE, evas_image_sink_cb_resize_event);
			if (esink->epipe) {
				ecore_pipe_del (esink->epipe);
				esink->epipe = NULL;
			}
			break;
		default:
			break;
	}

	return ret_state;
}

static void
gst_evas_image_sink_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GstEvasImageSink *esink = GST_EVASIMAGESINK (object);
	Evas_Object *eo;

	g_mutex_lock (instance_lock);

	switch (prop_id) {
	case PROP_EVAS_OBJECT:
		eo = g_value_get_pointer (value);
		if (is_evas_image_object (eo)) {
			if (eo != esink->eo) {
				/* delete evas object callbacks registrated on a former evas image object */
				evas_object_event_callback_del (esink->eo, EVAS_CALLBACK_DEL, evas_image_sink_cb_del_eo);
				evas_object_event_callback_del (esink->eo, EVAS_CALLBACK_RESIZE, evas_image_sink_cb_resize_event);

				esink->eo = eo;

				/* add evas object callbacks on a new evas image object */
				evas_object_event_callback_add (esink->eo, EVAS_CALLBACK_DEL, evas_image_sink_cb_del_eo, esink);
				evas_object_event_callback_add (esink->eo, EVAS_CALLBACK_RESIZE, evas_image_sink_cb_resize_event, esink);

				esink->gl_zerocopy = is_zerocopy_supported (evas_object_evas_get (eo));
				if (esink->gl_zerocopy) {
					evas_object_image_content_hint_set (esink->eo, EVAS_IMAGE_CONTENT_HINT_DYNAMIC);
					GST_DEBUG("Enable gl zerocopy");
				}
				GST_DEBUG("Evas Image Object(%x) is set",esink->eo);
				evas_object_show(esink->eo);
				esink->object_show = TRUE;
			}
		} else {
			GST_ERROR ("Cannot set evas-object property: value is not an evas image object");
		}
		break;

	case PROP_EVAS_OBJECT_SHOW:
		esink->object_show = g_value_get_boolean (value);
		if( !is_evas_image_object(esink->eo) ) {
			GST_WARNING ("Cannot apply visible(show-object) property: cannot get an evas object\n");
			break;
		}
		if(!esink->object_show) {
			evas_object_hide(esink->eo);
			GST_INFO ("object hide..");
		} else {
			evas_object_show(esink->eo);
			GST_INFO ("object show..");
		}
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}

	g_mutex_unlock (instance_lock);
}

static void
gst_evas_image_sink_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GstEvasImageSink *esink = GST_EVASIMAGESINK (object);

	switch (prop_id) {
	case PROP_EVAS_OBJECT:
		g_value_set_pointer (value, esink->eo);
		break;
	case PROP_EVAS_OBJECT_SHOW:
		g_value_set_boolean (value, esink->object_show);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static gboolean
gst_evas_image_sink_set_caps (GstBaseSink *base_sink, GstCaps *caps)
{
	int r;
	int w, h;
	GstEvasImageSink *esink = GST_EVASIMAGESINK (base_sink);

	esink->is_evas_object_size_set = FALSE;
	r = evas_image_sink_get_size_from_caps (caps, &w, &h);
	if (!r) {
		esink->w = w;
		esink->h = h;
		GST_DEBUG ("set size w(%d), h(%d)", w, h);
	}
	return TRUE;
}

static GstFlowReturn
gst_evas_image_sink_show_frame (GstVideoSink *video_sink, GstBuffer *buf)
{
	GstEvasImageSink *esink = GST_EVASIMAGESINK (video_sink);
	Eina_Bool r;

	g_mutex_lock (instance_lock);
	if (esink->present_data_addr == -1) {
		/* if present_data_addr is -1, we don't use this member variable */
	} else if (esink->present_data_addr != GST_BUFFER_DATA (buf)) {
		GST_WARNING ("skip rendering this buffer, present_data_addr:%x, GST_BUFFER_DATA(buf):%x", esink->present_data_addr,GST_BUFFER_DATA(buf));
		g_mutex_unlock (instance_lock);
		return;
	}
	if (!esink->epipe) {
		esink->epipe = ecore_pipe_add (evas_image_sink_cb_pipe, esink);
		if (!esink->epipe) {
			GST_ERROR ("ecore-pipe create failed");
			return GST_FLOW_ERROR;
		}
	}
	if (esink->object_show) {
		gst_buffer_ref (buf);
		__ta__("evasimagesink ecore_pipe_write", r = ecore_pipe_write (esink->epipe, &buf, sizeof (GstBuffer *)););
		if (r == EINA_FALSE)  {
			gst_buffer_unref (buf);
		}
		GST_DEBUG ("after ecore_pipe_write()");
	} else {
		GST_DEBUG ("skip ecore_pipe_write()");
	}
	g_mutex_unlock (instance_lock);
	return GST_FLOW_OK;
}

static gboolean
evas_image_sink_init (GstPlugin *evasimagesink)
{
	GST_DEBUG_CATEGORY_INIT (gst_evas_image_sink_debug, "evasimagesink", 0, "Evas image object based videosink");

	return gst_element_register (evasimagesink, "evasimagesink", GST_RANK_NONE, GST_TYPE_EVASIMAGESINK);
}

#ifndef PACKAGE
#define PACKAGE "gstevasimagesink-plugin-package"
#endif

GST_PLUGIN_DEFINE (
	GST_VERSION_MAJOR,
	GST_VERSION_MINOR,
	"evasimagesink",
	"Evas image object based videosink",
	evas_image_sink_init,
	VERSION,
	"LGPL",
	"Samsung Electronics Co",
	"http://www.samsung.com/"
)
