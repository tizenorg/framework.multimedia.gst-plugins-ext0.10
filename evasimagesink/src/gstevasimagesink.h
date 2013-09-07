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

#ifndef __GST_EVASIMAGESINK_H__
#define __GST_EVASIMAGESINK_H__

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <Evas.h>
#include <Ecore.h>
#include <mm_ta.h>

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_EVASIMAGESINK \
  (gst_evas_image_sink_get_type())
#define GST_EVASIMAGESINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_EVASIMAGESINK,GstEvasImageSink))
#define GST_EVASIMAGESINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_EVASIMAGESINK,GstEvasImageSinkClass))
#define GST_IS_EVASIMAGESINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_EVASIMAGESINK))
#define GST_IS_EVASIMAGESINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_EVASIMAGESINK))

typedef struct _GstEvasImageSink      GstEvasImageSink;
typedef struct _GstEvasImageSinkClass GstEvasImageSinkClass;

struct _GstEvasImageSink
{
	GstVideoSink element;

	Evas_Object *eo;
	Ecore_Pipe *epipe;
	Evas_Coord w;
	Evas_Coord h;
	gboolean object_show;
	gchar update_visibility;
	gboolean gl_zerocopy;

	GstBuffer *oldbuf;

	gboolean is_evas_object_size_set;
	guint present_data_addr;
};

struct _GstEvasImageSinkClass
{
	GstVideoSinkClass parent_class;
};

GType gst_evas_image_sink_get_type (void);

G_END_DECLS

#endif /* __GST_EVASIMAGESINK_H__ */
