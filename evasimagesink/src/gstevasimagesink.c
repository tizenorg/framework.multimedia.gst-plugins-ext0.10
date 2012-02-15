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
#  include <config.h>
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

static void gst_evas_image_sink_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_evas_image_sink_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static gboolean gst_evas_image_sink_set_caps (GstBaseSink * base_sink, GstCaps * caps);
static GstFlowReturn gst_evas_image_sink_show_frame (GstVideoSink * video_sink, GstBuffer * buf);

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
gst_evas_image_sink_class_init (GstEvasImageSinkClass * klass)
{
	GObjectClass *gobject_class;
	GstBaseSinkClass *gstbasesink_class;
	GstVideoSinkClass *gstvideosink_class;

	gobject_class = (GObjectClass *) klass;
	gstbasesink_class = GST_BASE_SINK_CLASS (klass);
	gstvideosink_class = GST_VIDEO_SINK_CLASS (klass);

	gobject_class->set_property = gst_evas_image_sink_set_property;
	gobject_class->get_property = gst_evas_image_sink_get_property;

	g_object_class_install_property (gobject_class, PROP_EVAS_OBJECT,
		g_param_spec_pointer ("evas-object", "Destination Evas Object",	"Destination evas image object", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_EVAS_OBJECT_SHOW,
		g_param_spec_boolean ("visible", "Show Evas Object", "When disabled, evas object does not show", TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	gstvideosink_class->show_frame = gst_evas_image_sink_show_frame;
	gstbasesink_class->set_caps = gst_evas_image_sink_set_caps;
}

static void
gst_evas_image_sink_fini (gpointer data, GObject *obj)
{
	GstEvasImageSink *esink = GST_EVASIMAGESINK (obj);
	if (!esink) {
		return;
	}
	if (esink->epipe) {
		ecore_pipe_del (esink->epipe);
	}
	if (esink->oldbuf) {
		gst_buffer_unref (esink->oldbuf);
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

	memcpy (&buf, buffer, sizeof (GstBuffer *));
	if (!buf) {
		GST_ERROR ("There is no buffer\n");
		return;
	}

	if (esink->gl_zerocopy) {
		img_data = evas_object_image_data_get (esink->eo, EINA_TRUE);
		if (!img_data || !GST_BUFFER_DATA(buf)) {
			GST_WARNING ("Cannot get image data from evas object or cannot get gstbuffer data\n");
			evas_object_image_data_set(esink->eo, img_data);
		} else {
			GST_DEBUG ("img_data(%x), GST_BUFFER_DATA(buf):%x, esink->w(%d),esink->h(%d)",img_data,GST_BUFFER_DATA(buf),esink->w,esink->h);
			memcpy (img_data, GST_BUFFER_DATA (buf), esink->w * esink->h * COLOR_DEPTH);
			evas_object_image_pixels_dirty_set (esink->eo, 1);
			evas_object_image_data_set(esink->eo, img_data);
		}
		gst_buffer_unref (buf);
	} else {
		evas_object_image_data_set (esink->eo, GST_BUFFER_DATA (buf));
		evas_object_image_pixels_dirty_set (esink->eo, 1);
		if (esink->oldbuf) {
			gst_buffer_unref(esink->oldbuf);
		}
		esink->oldbuf = buf;
	}
}

static void
gst_evas_image_sink_init (GstEvasImageSink * esink, GstEvasImageSinkClass * gclass)
{
	esink->eo = NULL;
	g_object_weak_ref (G_OBJECT (esink), gst_evas_image_sink_fini, NULL);
}

static void
evas_image_sink_cb_del_eo (void *data, Evas *e, Evas_Object *obj, void *event_info)
{
	GstEvasImageSink *esink = data;
	if (!esink) {
		return;
	}
	if (esink->oldbuf) {
		gst_buffer_unref (esink->oldbuf);
		esink->oldbuf = NULL;
		esink->eo = NULL;
	}
	ecore_pipe_del (esink->epipe);
	esink->epipe = NULL;
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

static void
gst_evas_image_sink_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
	GstEvasImageSink *esink = GST_EVASIMAGESINK (object);
	Evas_Object *eo;

	switch (prop_id) {
	case PROP_EVAS_OBJECT:
		eo = g_value_get_pointer (value);
		if (is_evas_image_object (eo)) {
			esink->eo = eo;
			evas_object_event_callback_add (esink->eo, EVAS_CALLBACK_DEL, evas_image_sink_cb_del_eo, esink);
			if (esink->w > 0 && esink->h >0) {
				evas_object_image_size_set (esink->eo, esink->w, esink->h);
			}
			esink->gl_zerocopy = is_zerocopy_supported (evas_object_evas_get (eo));
			if (esink->gl_zerocopy) {
				evas_object_image_content_hint_set (esink->eo, EVAS_IMAGE_CONTENT_HINT_DYNAMIC);
				GST_DEBUG("Enable gl zerocopy");
			}
			if (!esink->epipe) {
				esink->epipe = ecore_pipe_add (evas_image_sink_cb_pipe, esink);
			}
			if (!esink->epipe) {
				GST_ERROR ("Cannot set evas-object property: pipe create failed");
			}
			GST_DEBUG("property set, Evas Object is set");
			evas_object_show(esink->eo);
			esink->object_show = TRUE;
			GST_INFO ("object show..");
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
}

static void
gst_evas_image_sink_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
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
gst_evas_image_sink_set_caps (GstBaseSink * base_sink, GstCaps * caps)
{
	int r;
	int w, h;
	GstEvasImageSink *esink = GST_EVASIMAGESINK (base_sink);

	r = evas_image_sink_get_size_from_caps (caps, &w, &h);
	if (!r) {
		if (esink->eo) {
			evas_object_image_size_set (esink->eo, w, h);
			GST_DEBUG ("evas_object_image_size_set: w(%d) h(%d)", w, h);
		}
		esink->w = w;
		esink->h = h;
	}
	return TRUE;
}

static GstFlowReturn
gst_evas_image_sink_show_frame (GstVideoSink * video_sink, GstBuffer * buf)
{
	GstEvasImageSink *esink = GST_EVASIMAGESINK (video_sink);
	Eina_Bool r;
	if (esink->epipe && esink->object_show) {
		gst_buffer_ref (buf);
		r = ecore_pipe_write (esink->epipe, &buf, sizeof (GstBuffer *));
		if (r == EINA_FALSE)  {
			gst_buffer_unref (buf);
		}
		GST_DEBUG ("after ecore_pipe_write()");
	}
	return GST_FLOW_OK;
}

static gboolean
evas_image_sink_init (GstPlugin * evasimagesink)
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
