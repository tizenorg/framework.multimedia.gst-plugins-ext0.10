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


#if defined(_MM_PROJECT_FLOATER) || defined(_MM_PROJECT_PROTECTOR) || defined(_MM_PROJECT_VOLANS)
#define VERSION "0.10.19" // sec
#define PACKAGE "gstreamer"
#elif defined(_MM_PROJECT_ADORA)
#define VERSION "0.10.9" // mavell
#define PACKAGE "gstreamer"
#else
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include "gstavsysmemsink.h"

#define debug_enter g_print
#define debug_leave g_print
#define debug_fenter() g_print("enter: %s\n",__FUNCTION__)
#define debug_fleave() g_print("leave: %s\n",__FUNCTION__)
#define debug_msg g_print
#define debug_verbose g_print
#define debug_warning g_print
#define debug_error g_print

#define GST_CAT_DEFAULT avsysmemsink_debug

GST_DEBUG_CATEGORY_STATIC (avsysmemsink_debug);

enum 
{
    SIGNAL_VIDEO_STREAM,
    LAST_SIGNAL
};

enum 
{
	PROP_0,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_ROTATE,
};

static GstStaticPadTemplate sink_factory = 
        GST_STATIC_PAD_TEMPLATE ("sink",
                                GST_PAD_SINK, GST_PAD_ALWAYS, 
                                GST_STATIC_CAPS (
                                    "video/x-raw-yuv, "
                                    "framerate = (fraction) [ 0, MAX ], "
                                    "width = (int) [ 1, MAX ], "
                                    "height = (int) [ 1, MAX ], "
                                    "format = (fourcc) {YV12};"
					    			"video/x-raw-rgb,"
                                    "bpp = (int)32,"
                                    "depth = (int)24")
                            );

static GstElementDetails AvsysMemSink_details = {
            "AV-system Stream callback",
            "Sink/Video", 
            "Stream sink for AV-System GStreamer Plug-in", 
            ""
};

static guint gst_avsysmemsink_signals[LAST_SIGNAL] = { 0 };


#ifdef G_ENABLE_DEBUG
#define g_marshal_value_peek_int(v)      g_value_get_int (v)
#define g_marshal_value_peek_pointer(v)  g_value_get_pointer (v)
#else /* !G_ENABLE_DEBUG */
#define g_marshal_value_peek_int(v)      (v)->data[0].v_int
#define g_marshal_value_peek_pointer(v)  (v)->data[0].v_pointer
#endif /* !G_ENABLE_DEBUG */


#define     GET_PIXEL(buf, x, y, stride)  (*((unsigned char*)(buf) + (x) + (y)*(stride)))

#define     GET_RED(Y,U,V)  	((9535 * (Y - 16) + 13074 * (V - 128)) >> 13)
#define     GET_GREEN(Y,U,V)  ((9535 * (Y - 16) - 6660 * (V - 128) - 3203 * (U - 128)) >> 13 )
#define     GET_BLUE(Y,U,V)   ((9535 * (Y - 16) + 16531 * (U - 128)) >> 13 )

#define     UCLIP(a) (((a)<0)?0:((a)>255)?255:(a))


static void
yuv420toargb(unsigned char *src, unsigned char *dst, int width, int height)
{
    int h,w;
    int y=0,u=0,v=0;
    int a=0,r=0,g=0,b=0;
    
    unsigned char* pixel;

    int index=0;

    unsigned char 	*pY;
    unsigned char 	*pU;
    unsigned char 	*pV;

    pY = src ;
    pU = src + (width * height) ;
    pV = src + (width * height) + (width * height /4) ;

    a = 255;

    for(h = 0 ; h < height; h++)
    {
        for(w = 0 ; w < width; w++)
        {
            y = GET_PIXEL(pY,w,h,width);
            u = GET_PIXEL(pU,w/2,h/2,width/2);
            v = GET_PIXEL(pV,w/2,h/2,width/2);

            r = GET_RED(y,u,v);
            g = GET_GREEN(y,u,v);
            b = GET_BLUE(y,u,v);

            r = UCLIP(r);
            g = UCLIP(g);
            b = UCLIP(b);

            index = (w + (h* width)) * 4;
            dst[index] = r;
            dst[index+1] = g;
            dst[index+2] = b;
            dst[index+3] = a;
        }
    }
}

static void
rotate_pure(unsigned char *src, unsigned char *dst, int width,int height,int angle,int bpp)
{

    int     size;
    int     new_x,new_y;
    int     org_x,org_y;
    int     dst_width;
    int     src_idx, dst_idx;
    
    size = width * height * bpp;
    
    if(angle == 0)
    {
        memcpy(dst,src,size);
        return;
    }

    for(org_y =0; org_y < height; org_y++)
    {
        for(org_x = 0; org_x < width ; org_x++)
        {
            if(angle == 90)
            {
                new_x = height - org_y;
                new_y = org_x;

                dst_width = height;
            }
            else if(angle == 180)
            {
                new_x = width - org_x;
                new_y = height - org_y;
                dst_width = width;
            }
            else if(angle == 270)
            {
                new_x = org_y;
                new_y = width - org_x;
                dst_width = height;
            }
            else
            {
                g_print("Not support Rotate : %d\n",angle);
                return;
            }

            src_idx = org_x + (org_y * width);
            dst_idx = new_x + (new_y * dst_width);

            memcpy(dst + (dst_idx*bpp), src+(src_idx *bpp),bpp);
        }
    }
    
}

static void
resize_pure(unsigned char *src, unsigned char *dst, int src_width, int src_height, int dst_width,int dst_height, int bpp)
{
    float 	xFactor,yFactor;

    float		org_fx,org_fy;
    int		org_x,org_y;

    int		x,y;

    int		src_index,dst_index;

    unsigned short *pshortSrc;
    unsigned short *pshortDst;

    if(bpp == 2)
    {
        pshortSrc = (unsigned short*)src;
        pshortDst = (unsigned short*)dst;
    }

    xFactor = (float)((dst_width<<16) / src_width);
    yFactor = (float)((dst_height<<16) / src_height);

    for(y = 0; y < dst_height; y++)
    {
        for(x = 0; x < dst_width; x++)
        {
            org_fx = (float)((x<<16)/xFactor);
            org_fy = (float)((y<<16)/yFactor);

            org_x = (int)(org_fx);
            org_y = (int)(org_fy);

            src_index = org_x + (org_y * src_width);
            dst_index = x + (y*dst_width);

            memcpy(dst+(dst_index *bpp ),src+(src_index *bpp),bpp);
        }
    }
}

/* BOOLEAN:POINTER,INT,INT (avsysvideosink.c:1) */
void
gst_avsysmemsink_BOOLEAN__POINTER_INT_INT (GClosure         *closure,
                                             GValue         *return_value G_GNUC_UNUSED,
                                             guint          n_param_values,
                                             const GValue   *param_values,
                                             gpointer       invocation_hint G_GNUC_UNUSED,
                                             gpointer       marshal_data)
{
    typedef gboolean (*GMarshalFunc_BOOLEAN__POINTER_INT_INT) (gpointer     data1,
                                                                gpointer     arg_1,
                                                                gint         arg_2,
                                                                gint         arg_3,
                                                                gpointer     data2);
    register GMarshalFunc_BOOLEAN__POINTER_INT_INT callback;
    register GCClosure *cc = (GCClosure*) closure;
    register gpointer data1, data2;

    gboolean v_return;

    g_return_if_fail (return_value != NULL);
    g_return_if_fail (n_param_values == 4);

    if (G_CCLOSURE_SWAP_DATA (closure))
    {
        data1 = closure->data;
        data2 = g_value_peek_pointer (param_values + 0);
    }
    else
    {
        data1 = g_value_peek_pointer (param_values + 0);
        data2 = closure->data;
    }
    callback = (GMarshalFunc_BOOLEAN__POINTER_INT_INT) (marshal_data ? marshal_data : cc->callback);

    v_return = callback (data1,
                        g_marshal_value_peek_pointer (param_values + 1),
                        g_marshal_value_peek_int (param_values + 2),
                        g_marshal_value_peek_int (param_values + 3),
                        data2);

    g_value_set_boolean (return_value, v_return);
}

static void gst_avsysmemsink_init_interfaces (GType type);


GST_BOILERPLATE_FULL (GstAvsysMemSink, gst_avsysmemsink, GstVideoSink, GST_TYPE_VIDEO_SINK, gst_avsysmemsink_init_interfaces);


static void 
gst_avsysmemsink_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GstAvsysMemSink *AvsysMemSink = GST_AVSYS_MEM_SINK (object);
	switch (prop_id) {
		case PROP_0:
			break;
		case PROP_WIDTH:
               if(AvsysMemSink->dst_width != g_value_get_int (value))
               {
        			AvsysMemSink->dst_width = g_value_get_int (value);
                    AvsysMemSink->dst_changed = 1;
               }
			break;
		case PROP_HEIGHT:
			if(AvsysMemSink->dst_height != g_value_get_int (value))
			{
        			AvsysMemSink->dst_height = g_value_get_int (value);
                    AvsysMemSink->dst_changed = 1;
			}
			break;
          case PROP_ROTATE:
               if(AvsysMemSink->rotate != g_value_get_int(value))
               {
                   AvsysMemSink->rotate = g_value_get_int(value);
                   AvsysMemSink->dst_changed = 1;
               }
               break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			g_print ("invalid property id\n");
			break;
	};
}

static void 
gst_avsysmemsink_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GstAvsysMemSink *AvsysMemSink = GST_AVSYS_MEM_SINK (object);

	switch (prop_id) {
		case PROP_0:
			break;
		case PROP_WIDTH:
			g_value_set_int (value, AvsysMemSink->dst_width);
			break;
		case PROP_HEIGHT:
			g_value_set_int (value, AvsysMemSink->dst_height);
			break;
          case PROP_ROTATE:
               g_value_set_int (value, AvsysMemSink->rotate);
               break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			debug_warning ("invalid property id\n");
			break;
	}
}

static void
free_buffer(GstAvsysMemSink *AvsysMemSink)
{
    if(AvsysMemSink->con_buf)
        free(AvsysMemSink->con_buf);

    if(AvsysMemSink->rot_buf)
        free(AvsysMemSink->rot_buf);

    if(AvsysMemSink->rsz_buf)
        free(AvsysMemSink->rsz_buf);

    AvsysMemSink->con_buf = NULL;
    AvsysMemSink->rot_buf = NULL;
    AvsysMemSink->rsz_buf = NULL;

}
static GstStateChangeReturn 
gst_avsysmemsink_change_state (GstElement *element, GstStateChange transition)
{
	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
	GstAvsysMemSink *AvsysMemSink = GST_AVSYS_MEM_SINK (element);
	switch (transition) {
		case GST_STATE_CHANGE_NULL_TO_READY:
			debug_msg ("GST AVSYS DISPLAY SINK: NULL -> READY\n");
			break;
		case GST_STATE_CHANGE_READY_TO_PAUSED:
			debug_msg ("GST AVSYS DISPLAY SINK: READY -> PAUSED\n");
			break;
		case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
			debug_msg ("GST AVSYS DISPLAY SINK: PAUSED -> PLAYING\n");
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
			debug_msg ("GST AVSYS MEM SINK: PLAYING -> PAUSED\n");
			break;
		case GST_STATE_CHANGE_PAUSED_TO_READY:
			debug_msg ("GST AVSYS MEM SINK: PAUSED -> READY\n");
               free_buffer(AvsysMemSink);        
			break;
		case GST_STATE_CHANGE_READY_TO_NULL:
			debug_msg ("GST AVSYS MEM SINK: READY -> NULL\n");
			break;
		default:
			break;
	}

	return ret;


}


static gboolean
gst_avsysmemsink_set_caps (GstBaseSink * bs, GstCaps * caps)
{
    GstAvsysMemSink *s = GST_AVSYS_MEM_SINK (bs);
    GstStructure *structure;
    gint width = 0;
    gint height = 0;

    if (caps != NULL)
    {
        guint32 fourcc;
        char *name = NULL;
        int bpp = 0, depth = 0;

        structure = gst_caps_get_structure (caps, 0);

        /**/
        name = (char *) gst_structure_get_name (structure);
        debug_msg ("CAPS NAME: %s\n", name);

		if (gst_structure_has_name (structure, "video/x-raw-rgb"))
		{
			s->is_rgb = TRUE;
		}
		else if (gst_structure_has_name (structure, "video/x-raw-yuv")) 
		{
			s->is_rgb = FALSE;
		}

        /* get source size */
        gst_structure_get_int (structure, "height", &height);
        gst_structure_get_int (structure, "width", &width);

        if (gst_structure_get_fourcc (structure, "format", &fourcc)) 
        {
            switch (fourcc) 
            {
                case GST_MAKE_FOURCC ('U', 'Y', 'V', 'Y'):
                    debug_warning("set format AVSYS_VIDEO_FORMAT_UYVY\n");
                    break;
                case GST_MAKE_FOURCC ('Y', 'V', '1', '6'):
                case GST_MAKE_FOURCC ('Y', '4', '2', 'B'):
                    debug_warning("set format AVSYS_VIDEO_FORMAT_YUV422\n");
                    break;
                case GST_MAKE_FOURCC ('Y', 'V', '1', '2'):
                    debug_warning("set format AVSYS_VIDEO_FORMAT_YUV420\n");
                    break;
                case GST_MAKE_FOURCC ('R', 'G', 'B', ' '):
                    debug_warning("set format AVSYS_VIDEO_FORMAT_RGB565\n");
                    break;
                default:
                    debug_warning("set default format AVSYS_VIDEO_FORMAT_RGB565\n");
                    break;
            }
        }

        if( s->src_width != width || 
            s->src_height != height) 
        {
            debug_warning ("DISPLAY: Video Source Changed! [%d x %d] -> [%d x %d]\n",
                                    s->src_width, s->src_height, width, height);
            s->src_changed = TRUE;
        }

        s->src_width = width;
        s->src_height = height;

        if(s->dst_width ==0)
        {
            s->dst_width = width;
            s->dst_changed = 1;
        }

        if(s->dst_height == 0)
        {
            s->dst_height = height;
            s->dst_changed = 1;            
        }
        debug_msg ("SRC CAPS: width:%d, height:%d \n", width, height);
    } 
    else 
    {
        debug_warning ("caps is NULL.\n");
    }

    debug_fleave ();

    return TRUE;
}

static GstFlowReturn
gst_avsysmemsink_preroll (GstBaseSink * bsink, GstBuffer * buf)
{
	GstAvsysMemSink *AvsysMemSink = GST_AVSYS_MEM_SINK (bsink);

	AvsysMemSink->src_length = GST_BUFFER_SIZE (buf);
	debug_msg ("SRC LENGTH: %d\n", AvsysMemSink->src_length);

	return GST_FLOW_OK;
}

static GstFlowReturn
gst_avsysmemsink_show_frame (GstBaseSink * bsink, GstBuffer * buf)
{
	GstAvsysMemSink *s = GST_AVSYS_MEM_SINK (bsink);
	gboolean res = FALSE;
	int f_size;
	f_size = GST_BUFFER_SIZE (buf);
	unsigned char       *dst_buf;

	if ( ! s->is_rgb )
	{
	    if (s->dst_changed == TRUE)
	    {
	        if(s->con_buf)
	        {
	            free(s->con_buf);
	            s->con_buf = NULL;
	        }

	        if(s->rot_buf)
	        {
	            free(s->rot_buf);
	            s->rot_buf = NULL;
	        }

	        if(s->rsz_buf)
	        {
	            free(s->rsz_buf);
	            s->rsz_buf = NULL;
	        }

	        s->con_buf = malloc(s->src_width * s->src_height * 4);
	        if(s->rotate != 0)
	        {
	            s->rot_buf = malloc(s->src_width * s->src_height * 4);
	        }

	        s->rsz_buf = malloc(s->dst_width * s->dst_height *4);
	        
	        s->dst_changed = FALSE;
	    } 

	    yuv420toargb(GST_BUFFER_DATA (buf),s->con_buf,s->src_width, s->src_height);
	    if(s->rotate != 0)
	    {
	        rotate_pure(s->con_buf,s->rot_buf,s->src_width, s->src_height,s->rotate,4);
	        if(s->rotate == 90 || s->rotate == 270)
	        {
	            resize_pure(s->rot_buf,s->rsz_buf,s->src_height,s->src_width,
	                        s->dst_width, s->dst_height,4);
	        }
	        else
	        {
	            resize_pure(s->rot_buf,s->rsz_buf,s->src_width,s->src_height,
	                        s->dst_width, s->dst_height,4);
	        }
	    }
	    else
	    {
	        resize_pure(s->con_buf,s->rsz_buf,s->src_width,s->src_height,
	                        s->dst_width, s->dst_height,4);
	    }

	    /* emit signal for video-stream */
	    g_signal_emit (s,gst_avsysmemsink_signals[SIGNAL_VIDEO_STREAM],
	                    0,s->rsz_buf,
	                    s->dst_width,s->dst_height,
	                    &res);
	}
	else
	{	
		/* NOTE : video can be resized by convert plugin's set caps on running time. 
		 * So, it should notice it to application through callback func.
		 */
		 g_signal_emit (s, gst_avsysmemsink_signals[SIGNAL_VIDEO_STREAM],
		                    0, GST_BUFFER_DATA (buf),
		                    s->src_width, s->src_height,
		                    &res);
	 }

    /*check video stream callback result.*/
    if (res) 
    {
        //debug_verbose("Video stream is called.\n");
        return GST_FLOW_OK;
    }

    return GST_FLOW_OK;
}

static void
gst_avsysmemsink_init_interfaces (GType type)
{
	/*void*/
}


static void 
gst_avsysmemsink_base_init (gpointer klass)
{
	GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

	gst_element_class_add_pad_template (element_class, 
                                gst_static_pad_template_get (&sink_factory));
	gst_element_class_set_details (element_class, &AvsysMemSink_details);
}

static void 
gst_avsysmemsink_class_init (GstAvsysMemSinkClass *klass)
{
	GObjectClass *gobject_class  = (GObjectClass*) klass;
	GstElementClass *gstelement_class = (GstElementClass*) klass;
	GstBaseSinkClass *gstbasesink_class = (GstBaseSinkClass *) klass;

    
	parent_class = g_type_class_peek_parent (klass);

	gobject_class->set_property = gst_avsysmemsink_set_property;
	gobject_class->get_property = gst_avsysmemsink_get_property;


	g_object_class_install_property (gobject_class, PROP_WIDTH,
										g_param_spec_int ("width",
													"Width",
													"Width of display",
													0, G_MAXINT, 176,
													G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));


	g_object_class_install_property (gobject_class, PROP_HEIGHT,
										g_param_spec_int ("height",
													"Height",
													"Height of display",
													0, G_MAXINT, 144,
													G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_ROTATE,
										g_param_spec_int ("rotate",
													"Rotate",
													"Rotate of display",
													0, G_MAXINT, 0,
													G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));


	/**
	* GstAvsysVideoSink::video-stream:
	*/
	gst_avsysmemsink_signals[SIGNAL_VIDEO_STREAM] = g_signal_new (
    							"video-stream",
    							G_TYPE_FROM_CLASS (klass),
    							G_SIGNAL_RUN_LAST,
    							0,
    							NULL,
    							NULL,
    							gst_avsysmemsink_BOOLEAN__POINTER_INT_INT,
    							G_TYPE_BOOLEAN,
    							3,
    							G_TYPE_POINTER, G_TYPE_INT, G_TYPE_INT);

    gstelement_class->change_state = gst_avsysmemsink_change_state;

    gstbasesink_class->set_caps = gst_avsysmemsink_set_caps;
    gstbasesink_class->preroll = GST_DEBUG_FUNCPTR (gst_avsysmemsink_preroll);
    gstbasesink_class->render = GST_DEBUG_FUNCPTR (gst_avsysmemsink_show_frame);

    
    GST_DEBUG_CATEGORY_INIT (avsysmemsink_debug, 
                            "AvsysMemSink", 
                            0, 
                            "AV system based GStreamer Plug-in");
}


static void 
gst_avsysmemsink_init (GstAvsysMemSink *AvsysMemSink, GstAvsysMemSinkClass *klass)
{
    /*private*/
    AvsysMemSink->src_width = 0;
    AvsysMemSink->src_height = 0;

    AvsysMemSink->src_changed = 0;

    /*property*/
    AvsysMemSink->dst_width = 0;
    AvsysMemSink->dst_height = 0;

    AvsysMemSink->dst_changed = 0;

    AvsysMemSink->rotate = 0;

    AvsysMemSink->con_buf = NULL;
    AvsysMemSink->rot_buf = NULL;
    AvsysMemSink->rsz_buf = NULL;

	AvsysMemSink->is_rgb = FALSE;
}


/* EOF */
