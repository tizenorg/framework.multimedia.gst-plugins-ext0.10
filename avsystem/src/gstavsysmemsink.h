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

 
#ifndef __GST_AVSYSMEMSINK_H__
#define __GST_AVSYSMEMSINK_H__

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <gst/interfaces/xoverlay.h>

G_BEGIN_DECLS

#define GST_TYPE_AVSYS_MEM_SINK             (gst_avsysmemsink_get_type())
#define GST_AVSYS_MEM_SINK(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVSYS_MEM_SINK, GstAvsysMemSink))
#define GST_AVSYS_MEM_SINK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVSYS_MEM_SINK, GstAvsysMemSinkClass))
#define GST_IS_AVSYS_MEM_SINK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVSYS_MEM_SINK))
#define GST_IS_AVSYS_MEM_SINK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVSYS_MEM_SINK))


typedef struct _GstAvsysMemSink      GstAvsysMemSink;
typedef struct _GstAvsysMemSinkClass GstAvsysMemSinkClass;

struct _GstAvsysMemSink
{
    GstVideoSink        videosink;

    int                 src_width;
    int                 src_height;

    int                 src_changed;
    int                 src_length;


    int                 dst_width;
    int                 dst_height;

    int                 dst_length;
    int                 dst_changed;

    unsigned char       *con_buf;
    unsigned char       *rot_buf;
    unsigned char       *rsz_buf;

	int                 rotate;

	int 				is_rgb;
};

struct _GstAvsysMemSinkClass
{
	GstVideoSinkClass parent_class;
};

GType gst_avsysmemsink_get_type (void);

G_END_DECLS


#endif /* __GST_AVSYSMEMSINK_H__ */

/* EOF */
