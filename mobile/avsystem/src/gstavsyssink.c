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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include "gstavsysaudiosink.h"
#include "gstavsysmemsink.h"

GST_DEBUG_CATEGORY (avsystem_sink_debug);

static gboolean
plugin_init (GstPlugin *plugin)
{
	gboolean error;
	/*register the exact name you can find in the framework*/
	error = gst_element_register (plugin, "avsysaudiosink",
								  GST_RANK_PRIMARY + 100,
								  GST_TYPE_AVSYS_AUDIO_SINK);

	error = gst_element_register (plugin, "avsysmemsink",
								  GST_RANK_NONE,
								  GST_TYPE_AVSYS_MEM_SINK);

	if (!error)
		goto failed;

	GST_DEBUG_CATEGORY_INIT (avsystem_sink_debug, "avsystemsink", 0, "avsystem sink plugins");
	return TRUE;

failed:

	return FALSE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
		GST_VERSION_MINOR,
		"avsyssink",
		"AV system video/audio sink plug-in",
		plugin_init,
		PACKAGE_VERSION,
		"LGPL",
		"AV System Sink",
		"http://www.samsung.com")
