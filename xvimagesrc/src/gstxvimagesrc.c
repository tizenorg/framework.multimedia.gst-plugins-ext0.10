/*
 * xvimagesrc
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Hyunil Park <hyunil46.park@samsung.com>
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
* SECTION:element-xvimagesrc
*
* xvimagesrc captures frame buffer which includes the application data along with video layer data
* from the XServer and pushes the data to the downstream element.
*
*
* <refsect2>
* <title>Example launch line</title>
* |[
* gst-launch xvimagesrc ! "video/x-raw-yuv, width=720, height=1280, framerate=(fraction)30/1, format=(fourcc)ST12" ! fakesink
* ]| captures the frame buffer from the XServer and send the buffers to a fakesink.
* </refsect2>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstxvimagesrc.h"

/* headers for drm */
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <X11/Xmd.h>
#include <X11/Xlibint.h>
#include <dri2.h>
#include <drm.h>
#include <sys/time.h>
#include <sys/times.h>
#include <exynos_drm.h>

GST_DEBUG_CATEGORY_STATIC (xvimagesrc_debug);
#define GST_CAT_DEFAULT xvimagesrc_debug


#define GST_XV_IMAGE_SRC_CAPS                    \
  "video/x-raw-rgb, "                            \
  "bpp = (int) 32, "                             \
  "depth = (int) 24, "                           \
  "endianness = (int) BIG_ENDIAN, "              \
  "red_mask = (int) 0xFF000000, "                \
  "green_mask = (int) 0x00FF0000, "              \
  "blue_mask = (int) 0x0000FF00, "               \
  "width  = (int)  [ 16, 4096 ], "               \
  "height = (int)  [ 16, 4096 ], "               \
  "framerate = (fraction) [0/1, 2147483647/1];"  \
  "video/x-raw-yuv,"                             \
  "format = (fourcc) { SN12, ST12 }, "                 \
  "width = (int) [ 1, 4096 ], "                  \
  "height = (int) [ 1, 4096 ], "                 \
  "framerate = (fraction) [0/1, 2147483647/1];"  \

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS(GST_XV_IMAGE_SRC_CAPS)
);

enum
{
  PROP_0
};

enum
{
  VIDEO_TYPE_VIDEO_WITH_UI,
  VIDEO_TYPE_VIDEO_ONLY,
};

enum
{
  SIGNAL_VIDEO_WITH_UI = 0,
  SIGNAL_VIDEO_ONLY,
  SIGNAL_LAST
};
#define GEM_NAME_MAX                6

#define SCMN_CS_YUV420              1 /* Y:U:V 4:2:0 */
#define SCMN_CS_I420                SCMN_CS_YUV420 /* Y:U:V */
#define SCMN_CS_NV12                6
#define SCMN_CS_NV12_T64X32         11 /* 64x32 Tiled NV12 type */
#define SCMN_CS_UYVY                100
#define SCMN_CS_YUYV                101
#define SCMN_CS_YUY2                SCMN_CS_YUYV

/* max channel count *********************************************************/
#define SCMN_IMGB_MAX_PLANE         (4)

/* image buffer definition ***************************************************

    +------------------------------------------+ ---
    |                                          |  ^
    |     a[], p[]                             |  |
    |     +---------------------------+ ---    |  |
    |     |                           |  ^     |  |
    |     |<---------- w[] ---------->|  |     |  |
    |     |                           |  |     |  |
    |     |                           |        |
    |     |                           |  h[]   |  e[]
    |     |                           |        |
    |     |                           |  |     |  |
    |     |                           |  |     |  |
    |     |                           |  v     |  |
    |     +---------------------------+ ---    |  |
    |                                          |  v
    +------------------------------------------+ ---

    |<----------------- s[] ------------------>|
*/

typedef struct
{
  /* width of each image plane */
  int w[SCMN_IMGB_MAX_PLANE];
  /* height of each image plane */
  int h[SCMN_IMGB_MAX_PLANE];
  /* stride of each image plane */
  int s[SCMN_IMGB_MAX_PLANE];
  /* elevation of each image plane */
  int e[SCMN_IMGB_MAX_PLANE];
  /* user space address of each image plane */
  void *a[SCMN_IMGB_MAX_PLANE];
  /* physical address of each image plane, if needs */
  void *p[SCMN_IMGB_MAX_PLANE];
  /* color space type of image */
  int cs;
  /* left postion, if needs */
  int x;
  /* top position, if needs */
  int y;
  /* to align memory */
  int __dummy2;
  /* arbitrary data */
  int data[16];
  /* dmabuf fd */
  gint32 fd[SCMN_IMGB_MAX_PLANE];
  /* flag for buffer share */
  int buf_share_method;
} SCMN_IMGB;

typedef struct
{
  void  *address[GEM_NAME_MAX];
  int buffer_size[GEM_NAME_MAX];
  int name[GEM_NAME_MAX];
  gint32 fd[GEM_NAME_MAX];
  gint32 handle[GEM_NAME_MAX];
} GEM_MMAP;

typedef enum {
 BUF_SHARE_METHOD_PADDR = 0,
 BUF_SHARE_METHOD_FD
} buf_share_method_t;

#define DEFAULT_USER_AGENT           "GStreamer xvimagesrc "

static guint gst_xv_image_src_signals[SIGNAL_LAST] = { 0 };

#define BASE_TIME 33000
#define LIMIT_TIME -33000
//#define COUNT_FRAMES
#ifdef COUNT_FRAMES
gchar old_time[10] = {0, };
#endif
static gboolean error_caught = FALSE;

static void gst_xv_image_src_finalize (GObject * gobject);

static void gst_xv_image_src_set_property (GObject *object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_xv_image_src_get_property (GObject *object, guint prop_id, GValue * value, GParamSpec * pspec);
static GstFlowReturn gst_xv_image_src_create (GstPushSrc * psrc, GstBuffer ** outbuf);
static gboolean gst_xv_image_src_start (GstBaseSrc * bsrc);
static gboolean gst_xv_image_src_stop (GstBaseSrc * bsrc);
static gboolean gst_xv_image_src_get_size (GstBaseSrc * bsrc, guint64 * size);
static gboolean gst_xv_image_src_is_seekable (GstBaseSrc * bsrc);
static gboolean gst_xv_image_src_query (GstBaseSrc * bsrc, GstQuery * query);
static gboolean gst_xv_image_src_unlock (GstBaseSrc * bsrc);
static gboolean gst_xv_image_src_unlock_stop (GstBaseSrc * bsrc);
static gboolean gst_xv_image_src_setcaps (GstBaseSrc * bsrc, GstCaps * caps);

static tbm_bufmgr bufmgr_get (Display *dpy, Pixmap pixmap);
static int port_get (Display *dpy, unsigned int id);
static void pixmap_update (GstXVImageSrc * src,Display *dpy, tbm_bufmgr bufmgr, Pixmap pixmap,
               int x, int y, int width, int height);
static Pixmap pixmap_create (GstXVImageSrc * src, Display *dpy, int width, int height);

static void* gst_xv_image_src_update_thread (void * asrc);
static gboolean xvimagesrc_thread_start(GstXVImageSrc *src);
static void drm_init(GstXVImageSrc *src);
static void drm_finalize(GstXVImageSrc *src);
static gint32 drm_convert_gem_to_fd(int * gemname_cnt, int drm_fd, unsigned int name,  void * data, void **virtual_address);
static void gst_xv_get_image_sleep(void *asrc, double duration);

static void
_do_init (GType type)
{
  GST_DEBUG_CATEGORY_INIT (xvimagesrc_debug, "xvimagesrc", 0, "Xv image src");
}

GST_BOILERPLATE_FULL (GstXVImageSrc, gst_xv_image_src, GstPushSrc, GST_TYPE_PUSH_SRC, _do_init);

static void
gst_xv_image_src_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  gst_element_class_add_pad_template (element_class, gst_static_pad_template_get (&srctemplate));
  gst_element_class_set_details_simple (element_class, "XServer Display FB video source",
    "Source/video",
    "Receive frame buffer data from XServer and passes to next element",
    "");
}

static void
gst_xv_image_src_class_init (GstXVImageSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstPushSrcClass *gstpushsrc_class;
  gobject_class = (GObjectClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstpushsrc_class = (GstPushSrcClass *) klass;
  gobject_class->set_property = gst_xv_image_src_set_property;
  gobject_class->get_property = gst_xv_image_src_get_property;
  gobject_class->finalize = gst_xv_image_src_finalize;

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_xv_image_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_xv_image_src_stop);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_xv_image_src_unlock);
  gstbasesrc_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_xv_image_src_unlock_stop);
  gstbasesrc_class->get_size = GST_DEBUG_FUNCPTR (gst_xv_image_src_get_size);
  gstbasesrc_class->is_seekable = GST_DEBUG_FUNCPTR (gst_xv_image_src_is_seekable);
  gstbasesrc_class->query = GST_DEBUG_FUNCPTR (gst_xv_image_src_query);
  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_xv_image_src_setcaps);
  gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_xv_image_src_create);

  gst_xv_image_src_signals[SIGNAL_VIDEO_WITH_UI] =
     g_signal_new ("video-with-ui", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
     G_STRUCT_OFFSET (GstXVImageSrcClass, video_with_ui), NULL, NULL,
     g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0, G_TYPE_NONE);

  gst_xv_image_src_signals[SIGNAL_VIDEO_ONLY] =
     g_signal_new ("video-only", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
     G_STRUCT_OFFSET (GstXVImageSrcClass, video_only), NULL, NULL,
     g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0, G_TYPE_NONE);

}

static gboolean gst_xv_image_src_get_frame_size(int fourcc, int width, int height, unsigned int *outsize)
{
  switch (fourcc) {
/* case GST_MAKE_FOURCC('I','4','2','0'):	// V4L2_PIX_FMT_YUV420
    *outsize = GST_ROUND_UP_4 (width) * GST_ROUND_UP_2 (height);
    *outsize += 2 * ((GST_ROUND_UP_8 (width) / 2) * (GST_ROUND_UP_2 (height) / 2));
    break;*/
  case GST_MAKE_FOURCC('S','N','1','2'):	// V4L2_PIX_FMT_NV12 non-linear
     GST_INFO("SN12");
  case GST_MAKE_FOURCC('S','T','1','2'):	// V4L2_PIX_FMT_NV12 tiled non-linear
     GST_INFO("ST12");
    *outsize = GST_ROUND_UP_4 (width) * GST_ROUND_UP_2 (height);
    *outsize += (GST_ROUND_UP_4 (width) * height) / 2;
    break;
  case GST_MAKE_FOURCC('R','G','B','4'):
    /* jpeg size can't be calculated here. */
    *outsize = width * height * 4;
    break;
  default:
    /* unkown format!! */
    *outsize = 0;
    return FALSE;
  }
  return TRUE;
}

static gboolean
gst_xv_image_src_parse_caps (const GstCaps * caps, guint32 *fourcc,
  gint * width, gint * height, gint * rate_numerator, gint * rate_denominator, unsigned int *framesize)
{
  const GstStructure *structure;
  GstPadLinkReturn ret = TRUE;
  const GValue *framerate;
  const char *media_type = NULL;
  GST_DEBUG ("parsing caps");
  if (gst_caps_get_size (caps) < 1) return FALSE;
  GST_INFO("xvimagesrc src caps:%"GST_PTR_FORMAT, caps);
  structure = gst_caps_get_structure (caps, 0);
  ret = gst_structure_get_int (structure, "width", width);
  if(!ret) {
    GST_ERROR ("xvimagesrc width not specified in caps");
    goto error;
  }
  ret = gst_structure_get_int (structure, "height", height);
  if(!ret) {
    GST_ERROR ("xvimagesrc height not specified in caps");
    goto error;
  }
  media_type = gst_structure_get_name (structure);
  if(media_type == NULL) {
    GST_ERROR ("xvimagesrc media type not specified in caps");
    goto error;
  }
  framerate = gst_structure_get_value (structure, "framerate");
  if (framerate) {
    *rate_numerator = gst_value_get_fraction_numerator (framerate);
    *rate_denominator = gst_value_get_fraction_denominator (framerate);
  } else {
    GST_ERROR ("xvimagesrc frametype not specified in caps");
    goto error;
  }
  if (g_strcmp0 (media_type, "video/x-raw-rgb") == 0) {
    gst_xv_image_src_get_frame_size(FOURCC_RGB32, *width, *height, framesize);
    *fourcc = FOURCC_RGB32;
    GST_DEBUG ("Caps set to RGB32");
  } else if(g_strcmp0 (media_type, "video/x-raw-yuv") == 0) {

    GST_INFO ("media_type is video/x-raw-yuv");
    guint32 format = FOURCC_SN12;
    ret = gst_structure_get_fourcc (structure, "format", &format);
    if (!ret) GST_DEBUG ("xvimagesrc format not specified in caps, SN12 selected as default");
    ret = gst_xv_image_src_get_frame_size(format, *width, *height, framesize);
    if(!ret) {
      GST_ERROR ("xvimagesrc unsupported format type specified in caps");
      goto error;
    }
    *fourcc = format;
  }
  return TRUE;
/* ERRORS */
error:
    return FALSE;
}

static gboolean gst_xv_image_src_setcaps (GstBaseSrc * bsrc, GstCaps * caps)
{
  gboolean res = TRUE;
  gint width, height, rate_denominator, rate_numerator;
  GstXVImageSrc *src;
  src = GST_XV_IMAGE_SRC (bsrc);
  res = gst_xv_image_src_parse_caps (caps, &src->format_id, &width, &height,
    &rate_numerator, &rate_denominator, &src->framesize);
  if (res) {
    /* looks ok here */
    src->width = width;
    src->height = height;
    src->rate_numerator = rate_numerator;
    src->rate_denominator = rate_denominator;
    GST_DEBUG_OBJECT (src, "size %dx%d, %d/%d fps",
      src->width, src->height,
      src->rate_numerator, src->rate_denominator);
  }
  xvimagesrc_thread_start(src);
  return res;
}
static void
gst_xv_image_src_reset (GstXVImageSrc * src)
{
}

static void
gst_xv_image_src_init (GstXVImageSrc * src, GstXVImageSrcClass * g_class)
{
  src->format_id = 0;
  src->running_time = GST_CLOCK_TIME_NONE;
  src->frame_duration = GST_CLOCK_TIME_NONE;
  src->virtual = NULL;
  src->bo = NULL;
  src->dri2_buffers = NULL;
  src->queue_lock = g_mutex_new ();
  src->queue = g_queue_new ();
  src->queue_cond = g_cond_new ();
  src->cond_lock = g_mutex_new ();
  src->drm_fd = -1;
  src->current_data_type = VIDEO_TYPE_VIDEO_WITH_UI;
  src->new_data_type = VIDEO_TYPE_VIDEO_WITH_UI;
  src->get_image_overtime = 0;
  src->get_image_overtime_cnt = 0;
  src->gemname_cnt = 0;
  drm_init(src);
}

static void
gst_xv_image_src_finalize (GObject * gobject)
{
  GstXVImageSrc *src = GST_XV_IMAGE_SRC (gobject);
  GST_DEBUG_OBJECT (src, "finalize");
  drm_finalize(src);
  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}
static void drm_init(GstXVImageSrc *src)
{
  Display *dpy;
  int eventBase, errorBase;
  int dri2Major, dri2Minor;
  char *driverName, *deviceName;
  struct drm_auth auth_arg = {0};

  src->drm_fd = -1;
  dpy = XOpenDisplay(0);

  /* DRI2 */
  if (!DRI2QueryExtension(dpy, &eventBase, &errorBase)) {
    GST_ERROR("DRI2QueryExtension !!");
    return;
  }
  if (!DRI2QueryVersion(dpy, &dri2Major, &dri2Minor)) {
   GST_ERROR("DRI2QueryVersion !!");
   return;
  }
  if (!DRI2Connect(dpy, RootWindow(dpy, DefaultScreen(dpy)), &driverName, &deviceName)) {
    GST_ERROR("DRI2Connect !!");
    return;
  }
  GST_INFO("Open drm device : %s", deviceName);

  /* get the drm_fd though opening the deviceName */
  src->drm_fd = open(deviceName, O_RDWR);
  if (src->drm_fd < 0) {
    GST_ERROR("cannot open drm device (%s)", deviceName);
    return;
  }

  /* get magic from drm to authentication */
  if (ioctl(src->drm_fd, DRM_IOCTL_GET_MAGIC, &auth_arg)) {
    GST_ERROR("cannot get drm auth magic");
    close(src->drm_fd);
    src->drm_fd = -1;
    return;
  }
  if (!DRI2Authenticate(dpy, RootWindow(dpy, DefaultScreen(dpy)), auth_arg.magic)) {
    GST_ERROR("cannot get drm authentication from X");
    close(src->drm_fd);
    src->drm_fd = -1;
    return;
  }
}
static void drm_finalize(GstXVImageSrc *src)
{
  if (src->drm_fd >= 0) {
    close(src->drm_fd);
    src->drm_fd = -1;
  }
}
static gint32 drm_convert_gem_to_fd(int *gemname_cnt, int drm_fd, unsigned int name, void  *data, void **virtual_address)
{
  g_return_val_if_fail((data != NULL),0);
  int count=0;
  gint32 fd = 0;
  count = *gemname_cnt;
  GST_DEBUG("gamname_cnt = %d", count);
  GST_DEBUG("name = %u", name);


  GEM_MMAP *xv_gem_mmap = NULL;
  xv_gem_mmap = (GEM_MMAP *) data;
  if(count >=GEM_NAME_MAX)
    goto PASS;

  if (count < GEM_NAME_MAX ) {
    int i =0;
    for ( i =0 ; i < GEM_NAME_MAX ; i++) {
      if (name == xv_gem_mmap->name[i])
	goto PASS;
    }

    struct drm_prime_handle prime;
    struct drm_gem_open gem_open;
    struct drm_exynos_gem_mmap gem_mmap; //for virtual address

    memset (&gem_open, 0, sizeof (struct drm_gem_open));
    gem_open.name = name;
    if (ioctl(drm_fd, DRM_IOCTL_GEM_OPEN, &gem_open)) {
      GST_ERROR("Gem Open failed");
      return 0;
    }
    memset (&prime, 0, sizeof (struct drm_prime_handle));
    prime.handle = gem_open.handle;
    prime.flags = DRM_CLOEXEC;
    /*get gem_open handle*/
    xv_gem_mmap->handle[count] = gem_open.handle;
    GST_DEBUG("gem_open.handle =%d, xv_gem_mmap->handle[count]=%d", gem_open.handle, xv_gem_mmap->handle[count]);
    /*get virtual address */
    /*set name*/
    xv_gem_mmap->name[count] = name;
    memset (&gem_mmap, 0, sizeof (struct drm_exynos_gem_mmap));
    gem_mmap.handle = prime.handle;
    gem_mmap.size = gem_open.size;
    /*set size*/
    xv_gem_mmap->buffer_size[count] = gem_mmap.size;
    if (drmIoctl(drm_fd, DRM_IOCTL_EXYNOS_GEM_MMAP, &gem_mmap) !=0) {
      GST_ERROR("Gem mmap failed [handle %d, size %d]", gem_mmap.handle, gem_mmap.size);
      return 0;
    }
    /*set virtual address*/
    xv_gem_mmap->address[count] = (void *)(gem_mmap.mapped);
    GST_DEBUG ("%d - Virtual address[%d] = %p size=%d ", name, count, xv_gem_mmap->address[count],  xv_gem_mmap->buffer_size[count] );

    /*get fd*/
    if (ioctl(drm_fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &prime) < 0) {
      GST_ERROR("Gem Handle to Fd failed");
      return 0;
    }
    xv_gem_mmap->fd[count] = prime.fd;
    GST_DEBUG("fd = %d", xv_gem_mmap->fd[count]);
  }

  if (count < GEM_NAME_MAX) {
    count ++;
    *gemname_cnt = count;
  }

PASS:

  if (name == xv_gem_mmap->name[0]) {
    *virtual_address = xv_gem_mmap->address[0];
    fd = xv_gem_mmap->fd[0];
  } else if (name == xv_gem_mmap->name[1]) {
    *virtual_address = xv_gem_mmap->address[1];
     fd = xv_gem_mmap->fd[1];
  } else if (name == xv_gem_mmap->name[2]) {
    *virtual_address = xv_gem_mmap->address[2];
     fd = xv_gem_mmap->fd[2];
  } else if ( name == xv_gem_mmap->name[3]) {
    *virtual_address = xv_gem_mmap->address[3];
     fd = xv_gem_mmap->fd[3];
  } else if (name == xv_gem_mmap->name[4]) {
    *virtual_address = xv_gem_mmap->address[4];
     fd = xv_gem_mmap->fd[4];
  } else if ( name == xv_gem_mmap->name[5]) {
    *virtual_address = xv_gem_mmap->address[5];
     fd = xv_gem_mmap->fd[5];
  }

  GST_DEBUG("virtual_address = %p  fd = %d", *virtual_address, fd);
  return fd;
}
static void
gst_xv_image_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstXVImageSrc *src = GST_XV_IMAGE_SRC (object);
  GST_INFO ("set property function %x", src);
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
  return;
}

static void
gst_xv_image_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstXVImageSrc *src = GST_XV_IMAGE_SRC (object);
  GST_INFO ("get property function %x", src);
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static inline GstClockTime xvimagesrc_clock (GstXVImageSrc *src)
{
  struct timeval tv;
  gettimeofday (&tv, NULL);
  if(!src->base_time) {
    src->base_time = GST_TIMEVAL_TO_TIME(tv);
  }
  return (GstClockTime)(GST_TIMEVAL_TO_TIME(tv) - src->base_time);
}

static gboolean gst_xv_image_src_get_timeinfo(GstXVImageSrc *src, GstBuffer *buffer)
{
  int fps_nu = 0;
  int fps_de = 0;
  GstClockTime timestamp = GST_CLOCK_TIME_NONE;
  GstClockTime duration = GST_CLOCK_TIME_NONE;

  if (!src || !buffer) {
    GST_WARNING("Invalid pointer [hadle:%p, buffer:%p]", src, buffer);
    return FALSE;
  }
  timestamp = xvimagesrc_clock(src);
  /* if we have a framerate adjust timestamp for frame latency */
  if ((int)((float)src->rate_numerator / (float)src->rate_denominator) <= 0) {
    /*if fps is zero, auto fps mode*/
    fps_nu = 0;
    fps_de = 1;
  } else {
    fps_nu = 1;
    fps_de = (int)((float)src->rate_numerator / (float)src->rate_denominator);
  }
  if (fps_nu > 0 && fps_de > 0) {
    GstClockTime latency;
    latency = gst_util_uint64_scale_int(GST_SECOND, fps_nu, fps_de);
    duration = latency;
  }
  GST_BUFFER_TIMESTAMP(buffer) = timestamp;
  GST_BUFFER_DURATION(buffer) = duration;
  return TRUE;
}
#ifdef COUNT_FRAMES
static  int fps = 0;

static gchar *get_current_system_time()
{
	gchar target[10]={0,};
	time_t t;
	struct tm tm;

	t = time(NULL);
	tzset();
	/*localtimer_r : available since libc 5.2.5*/
	if(localtime_r(&t, &tm) == NULL) {
		return NULL;
	}
	snprintf(target, sizeof(target), "%02i:%02i:%02i", tm.tm_hour, tm.tm_min, tm.tm_sec);
	return g_strdup(target);
}
#endif
static void gst_xv_get_image_sleep(void *asrc, double duration)
{
    GstXVImageSrc *src = (GstXVImageSrc *)asrc;
    g_return_if_fail(src != NULL);
    double sleep_time = 0;
    sleep_time = (double)BASE_TIME - duration;
    if (sleep_time < 0) {
	src->get_image_overtime_cnt ++;
	src->get_image_overtime += sleep_time;
	if (src->get_image_overtime_cnt > 2)
	  src->get_image_overtime = 0;
	if (src->get_image_overtime <= (double)LIMIT_TIME)
	  src->get_image_overtime = 0;
    } else if (sleep_time > 0) {
       src->get_image_overtime_cnt = 0;
	sleep_time = sleep_time + src->get_image_overtime;
	src->get_image_overtime = (sleep_time < 0) ? sleep_time : 0;
	if (sleep_time >0) {
          usleep(sleep_time);
	}
    }
}
static GstFlowReturn
gst_xv_image_src_create (GstPushSrc * psrc, GstBuffer ** buffer)
{
  GstXVImageSrc *src;
  GstBuffer *outbuf = NULL;
  src = GST_XV_IMAGE_SRC (psrc);
  g_mutex_lock (src->queue_lock);
  if(g_queue_is_empty (src->queue)) {
    g_mutex_unlock (src->queue_lock);
    g_cond_wait(src->queue_cond, src->cond_lock);
    g_mutex_lock (src->queue_lock);
    outbuf = (GstBuffer *)g_queue_pop_head(src->queue);
    g_mutex_unlock (src->queue_lock);
  }
  else {
    GstBuffer *tempbuf = NULL;
    while((tempbuf = (GstBuffer *)g_queue_pop_head(src->queue)) != NULL) outbuf = tempbuf;    // To reduce latency, skipping the old frames and submitting only latest frames
    g_mutex_unlock (src->queue_lock);
  }
  if(outbuf == NULL) return GST_FLOW_ERROR;
  GST_INFO("Time stamp of the buffer is %"GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(outbuf)));
  *buffer = outbuf;
#ifdef COUNT_FRAMES
    gchar *current_time = NULL;
    current_time = 	get_current_system_time();
    if (strncmp(current_time, old_time, 10) == 0) {
	fps ++;
	strncpy(old_time, current_time, 10);
    } else {
       g_printf("xvimagesrc %s - fps : %d\n", old_time, fps);
       fps = 1;
	strncpy(old_time, current_time, 10);
    }
#endif

  return GST_FLOW_OK;
}

static gboolean xvimagesrc_thread_start(GstXVImageSrc *src)
{
  GError *error;
  if(!src->updates_thread) src->updates_thread = g_thread_create ((GThreadFunc) gst_xv_image_src_update_thread, src, TRUE, &error);
  else GST_LOG_OBJECT (src, "The thread function already running");
  return TRUE;
}

static gboolean
gst_xv_image_src_start (GstBaseSrc * bsrc)
{
  GstXVImageSrc *src = GST_XV_IMAGE_SRC (bsrc);
  if(src->format_id) xvimagesrc_thread_start(src);
  return TRUE;
}

gboolean signal_emit_func(void *asrc)
{
    GstXVImageSrc *src = (GstXVImageSrc *)asrc;
    if (src->current_data_type == VIDEO_TYPE_VIDEO_ONLY) {
      g_signal_emit (src, gst_xv_image_src_signals[SIGNAL_VIDEO_ONLY] , 0, NULL);
   } else if (src->current_data_type == VIDEO_TYPE_VIDEO_WITH_UI) {
     g_signal_emit (src, gst_xv_image_src_signals[SIGNAL_VIDEO_WITH_UI] , 0, NULL);
   }
    return FALSE;
}

static int
gst_xvimagesrc_handle_xerror (Display * display, XErrorEvent * xevent)
{
  char error_msg[1024];

  XGetErrorText (display, xevent->error_code, error_msg, 1024);
  GST_DEBUG ("XError. error: %s", error_msg);
  error_caught = TRUE;
  return 0;
}

//static gboolean first_frame=TRUE;
static void* gst_xv_image_src_update_thread (void * asrc)
{
  GstXVImageSrc *src = (GstXVImageSrc *)asrc;
  g_return_val_if_fail((src != NULL),NULL);

  struct drm_gem_close gem_close;
  gboolean drop_frame = FALSE;
  int i=0;
  GEM_MMAP *xv_gem_mmap = NULL;
  xv_gem_mmap = (GEM_MMAP *)malloc(sizeof(GEM_MMAP));
  g_return_val_if_fail((xv_gem_mmap != NULL),NULL);

  memset(xv_gem_mmap, 0, sizeof(GEM_MMAP));

  GST_LOG_OBJECT (src, "The thread function start");
  {
    int damage_err_base = 0;
    Atom atom_format = 0;
    src->dpy = XOpenDisplay (NULL);
    src->p = port_get (src->dpy, src->format_id);
    if (src->p < 0) goto finish;
    /*src->width and src->height is set by caps info */
    GST_DEBUG (" width and height of caps : %dx%d ", src->width, src->height);
    if (src->width == 0 || src->height == 0) goto finish;
    GST_DEBUG ("gst_xv_image_src_update_thread pixmap_create !!");
    src->pixmap = pixmap_create (src, src->dpy, src->width, src->height);
    GST_DEBUG ("gst_xv_image_src_update_thread pixmap_create !!");
    src->gc = XCreateGC (src->dpy, src->pixmap, 0, 0);
    GST_DEBUG ("gst_xv_image_src_update_thread XCreateGC !!");
    src->bufmgr = bufmgr_get (src->dpy, src->pixmap);
    if (!src->bufmgr) goto finish;
    GST_DEBUG ("gst_xv_image_src_update_thread bufmgr_get !!");
    if (!XDamageQueryExtension(src->dpy, &src->damage_base, &damage_err_base)) goto finish;
    GST_DEBUG ("gst_xv_image_src_update_thread XDamageQueryExtension !!");
    src->damage = XDamageCreate (src->dpy, src->pixmap, XDamageReportNonEmpty);
    atom_format = XInternAtom (src->dpy, "_USER_WM_PORT_ATTRIBUTE_FORMAT", False);
    src->atom_stream_off = XInternAtom (src->dpy, "_USER_WM_PORT_ATTRIBUTE_STREAM_OFF", False);
    XvSetPortAttribute (src->dpy, src->p, atom_format, src->format_id);
#if 0
    /*set Xorg write-back hz*/
    atom_fps = XInternAtom (src->dpy, "_USER_WM_PORT_ATTRIBUTE_FPS", False);
    GST_DEBUG("frame rate of caps %d", src->rate_numerator);
    XvSetPortAttribute (src->dpy, src->p, atom_fps, src->rate_numerator);
    GST_DEBUG ("gst_xv_image_src_update_thread XvSetPortAttribute !!");
#endif
  }
  while(!src->thread_return) {
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);
    double duration = 0;
    GstBuffer *outbuf = NULL;
    XEvent ev;
    int eventcount = 0;
    void *virtual_address = NULL;
    int (*handler) (Display *, XErrorEvent *);
    /*get data type*/
    Atom atom_data_type = XInternAtom (src->dpy, "_USER_WM_PORT_ATTRIBUTE_DATA_TYPE", False);
    XvGetPortAttribute (src->dpy, src->p, atom_data_type, &(src->new_data_type));
    if (src->current_data_type != src->new_data_type) {
          src->current_data_type = src->new_data_type;
	   GST_DEBUG("current_data_type : %s \n", (src->current_data_type)?"Video":"UI+Video");
	   g_timeout_add(1, signal_emit_func, src);
    }
    XSync(src->dpy, 0);
    error_caught = FALSE;
    GST_DEBUG ("gst_xv_image_src_update_thread XSetErrorHandler in !!");
    handler = XSetErrorHandler (gst_xvimagesrc_handle_xerror);
    GST_INFO ("gst_xv_image_src_update_thread XSetErrorHandler !!");
    GST_INFO ("gst_xv_image_src_update_thread XvGetStill in !!");
    XvGetStill (src->dpy, src->p, src->pixmap, src->gc, 0, 0, src->width, src->height, 0, 0, src->width, src->height);
    GST_INFO ("gst_xv_image_src_update_thread XvGetStill !!");
    XSync (src->dpy, 0);
    if (error_caught) {
       GST_ERROR("gst_xv_image_src_update_thread error_caught is TRUE");
	error_caught = FALSE;
	XSetErrorHandler(handler);
	continue;
    }
    /*reset error handler*/
    error_caught = FALSE;
    XSetErrorHandler(handler);

    GST_INFO ("gst_xv_image_src_update_thread XSync !!");
    eventcount = XPending (src->dpy);
    GST_INFO ("gst_xv_image_src_update_thread XPending %d",eventcount);
    XNextEvent (src->dpy, &ev); /* wating for x event */
    GST_INFO ("gst_xv_image_src_update_thread XNextEvent !!");
    if (ev.type == (src->damage_base + XDamageNotify)) {
      XDamageNotifyEvent *damage_ev = (XDamageNotifyEvent *)&ev;
    GST_INFO ("gst_xv_image_src_update_thread XDamageNotifyEvent");
      if (damage_ev->drawable == src->pixmap) {
        pixmap_update (src, src->dpy, src->bufmgr, src->pixmap,
        damage_ev->area.x,
        damage_ev->area.y,
        damage_ev->area.width,
        damage_ev->area.height);
	      GST_INFO("gst_xv_image_src_update_thread pixmap_update");
      }
      XDamageSubtract (src->dpy, src->damage, None, None );
      GST_INFO ("gst_xv_image_src_update_thread XDamageSubtract");
    }
    if(!src->virtual) continue;
    if(src->format_id == FOURCC_RGB32) {
      outbuf = gst_buffer_new_and_alloc (src->framesize);
      gst_buffer_set_caps (outbuf, GST_PAD_CAPS (GST_BASE_SRC_PAD (src)));
      memcpy(GST_BUFFER_DATA (outbuf), src->virtual, src->framesize);
    } else if (src->format_id == FOURCC_SN12) {
      XV_GETSTILL_DATA_PTR data = (XV_GETSTILL_DATA_PTR)src->virtual;
      int error = XV_GETSTILL_VALIDATE_DATA (data);
      outbuf = gst_buffer_new();
      gst_buffer_set_caps (outbuf, GST_PAD_CAPS (GST_BASE_SRC_PAD (src)));
      if (error == XV_HEADER_ERROR)
        GST_ERROR ("XV_HEADER_ERROR\n");
      else if (error == XV_VERSION_MISMATCH)
        GST_ERROR ("XV_VERSION_MISMATCH\n");
      else
      {
        SCMN_IMGB *psimgb = NULL;
        psimgb = (SCMN_IMGB *)malloc(sizeof(SCMN_IMGB));
        if (psimgb == NULL) {
          GST_ERROR_OBJECT(src, "failed to alloc SCMN_IMGB");
          return NULL;
        }
        memset(psimgb, 0x00, sizeof(SCMN_IMGB));
	 if (data->BufType == XV_BUF_TYPE_LEGACY) {
	   psimgb->p[0] = (void *)data->YBuf;
          psimgb->p[1] = (void *)data->CbBuf;
	   psimgb->buf_share_method = BUF_SHARE_METHOD_PADDR;
	   psimgb->a[0] = NULL;
          psimgb->a[1] = NULL;
	 } else if (data->BufType == XV_BUF_TYPE_DMABUF) {
          psimgb->fd[0] = drm_convert_gem_to_fd(&src->gemname_cnt, src->drm_fd, (void *)data->YBuf, xv_gem_mmap, &virtual_address);
	   psimgb->a[0] = virtual_address;
	   GST_DEBUG("YBuf gem to fd[0]=%d virtual_address = %p", psimgb->fd[0], psimgb->a[0]);
          psimgb->fd[1] = drm_convert_gem_to_fd(&src->gemname_cnt, src->drm_fd, (void *)data->CbBuf, xv_gem_mmap, &virtual_address);
	   psimgb->a[1] = virtual_address;
	   GST_DEBUG("CbBuf gem to fd[1]=%d  virtual_address = %p", psimgb->fd[1], psimgb->a[1]);
	   psimgb->buf_share_method = BUF_SHARE_METHOD_FD;
	 }
        psimgb->w[0] = src->width;
        psimgb->h[0] = src->height;
        psimgb->cs   = SCMN_CS_NV12;
        psimgb->w[1] = src->width;
        psimgb->h[1] = src->height >> 1;

        psimgb->s[0] = GST_ROUND_UP_16(psimgb->w[0]);
        psimgb->e[0] = GST_ROUND_UP_16(psimgb->h[0]);
        psimgb->s[1] = GST_ROUND_UP_16(psimgb->w[1]);
        psimgb->e[1] = GST_ROUND_UP_16(psimgb->h[1]);

        GST_BUFFER_MALLOCDATA(outbuf) = (unsigned char*)psimgb;
        GST_BUFFER_DATA(outbuf) = src->virtual;
      }
    }
  if(!outbuf) continue;
	GST_BUFFER_SIZE (outbuf) = src->framesize;
    drop_frame = FALSE;
    gst_xv_image_src_get_timeinfo(src,outbuf);
    if(GST_CLOCK_TIME_IS_VALID(src->running_time) && GST_CLOCK_TIME_IS_VALID(src->frame_duration)) {
      if(GST_BUFFER_TIMESTAMP(outbuf) < (src->running_time+src->frame_duration-6000000)) {
        //drop_frame = TRUE;
      }
    }
    //if(!GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(outbuf))) drop_frame = TRUE;
    //if(!first_frame && !GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(outbuf))) drop_frame = TRUE;

    if(!drop_frame) {
      src->running_time = GST_BUFFER_TIMESTAMP(outbuf);
      src->frame_duration = GST_BUFFER_DURATION(outbuf);
      //first_frame = FALSE;
      g_mutex_lock (src->queue_lock);
      g_queue_push_tail(src->queue, outbuf);
      g_mutex_unlock (src->queue_lock);
      g_cond_signal(src->queue_cond);
    } else {
      GST_ERROR ("gst_xv_image_src_update_thread frame drop !! newts: %"GST_TIME_FORMAT" oldts+dur-8ms: %"GST_TIME_FORMAT,
                    GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(outbuf)), GST_TIME_ARGS(src->running_time+src->frame_duration-6000000));
      gst_buffer_unref(outbuf);
    }
    update_done:
      if (src->virtual) tbm_bo_unmap(src->bo);
      src->virtual = NULL;
      if (src->bo) tbm_bo_unref(src->bo);
      src->bo = NULL;
      if (src->dri2_buffers) free(src->dri2_buffers);
      src->dri2_buffers = NULL;
      gettimeofday(&end_time, NULL);
      duration = ((double)(end_time.tv_sec* 1000000)+(double)(end_time.tv_usec))-((double)(start_time.tv_sec* 1000000)+(double)(start_time.tv_usec));
      gst_xv_get_image_sleep (src, duration);
      GST_INFO ("gst_xv_image_src_update_thread cleanup !!");
  }
  for ( i=0 ; i < GEM_NAME_MAX ; i++) {
    /*gem munmap*/
    if (xv_gem_mmap->address[i]) {
      if (-1 == munmap(xv_gem_mmap->address[i],xv_gem_mmap->buffer_size[i])) {
        GST_ERROR ("munmap failed");
	 return NULL;
      }
    }
    /*fd close*/
    if (xv_gem_mmap->fd[i]) {
	gem_close.handle = xv_gem_mmap->handle[i];
	if (ioctl(xv_gem_mmap->fd[i], DRM_IOCTL_GEM_CLOSE, &gem_close)) {
		GST_ERROR("Gem Close failed");
	}
    }
  }
  if (xv_gem_mmap) {
    free(xv_gem_mmap);
    xv_gem_mmap = NULL;
  }
  GST_LOG_OBJECT (src, "The thread function cleanup");
  if (src->atom_stream_off > 0) {
    XvSetPortAttribute (src->dpy, src->p, src->atom_stream_off, 1);
    src->atom_stream_off = 0;
  }
  if (src->bufmgr) {
    tbm_bufmgr_deinit (src->bufmgr);
    src->bufmgr = NULL;
  }
  if (src->p > 0) {
    XvUngrabPort (src->dpy, src->p, 0);
    src->p = 0;
  }
  if (src->gc) {
    XFreeGC (src->dpy, src->gc);
    src->gc = NULL;
  }
  if (src->pixmap > 0) {
    XFreePixmap (src->dpy, src->pixmap);
    src->pixmap = 0;
  }
  if (src->dpy) {
    XCloseDisplay (src->dpy);
    src->dpy = NULL;
  }
  GST_LOG_OBJECT (src, "The thread function stop");
  return NULL;
finish:
  GST_LOG_OBJECT (src, "The thread function Error cleanup");
  if (src->atom_stream_off > 0) {
    XvSetPortAttribute (src->dpy, src->p, src->atom_stream_off, 1);
    src->atom_stream_off = 0;
  }
  if (src->bufmgr) tbm_bufmgr_deinit (src->bufmgr);
  src->bufmgr = NULL;
  if (src->p > 0) XvUngrabPort (src->dpy, src->p, 0);
  src->p = 0;
  if (src->gc) XFreeGC (src->dpy, src->gc);
  src->gc = NULL;
  if (src->pixmap > 0) XFreePixmap (src->dpy, src->pixmap);
  src->pixmap = 0;
  if (src->dpy) XCloseDisplay (src->dpy);
  src->dpy = NULL;
  GST_LOG_OBJECT (src, "The thread function Error stop");
  return NULL;
}

static tbm_bufmgr bufmgr_get (Display *dpy, Pixmap pixmap)
{
  int screen;
  int drm_fd;
  tbm_bufmgr bufmgr;
  int eventBase, errorBase;
  int dri2Major, dri2Minor;
  char *driverName, *deviceName;
  drm_magic_t magic;

  screen = DefaultScreen(dpy);
  if (!DRI2QueryExtension (dpy, &eventBase, &errorBase)) {
    GST_ERROR ("!!Error : DRI2QueryExtension !!");
    return NULL;
  }
  if (!DRI2QueryVersion (dpy, &dri2Major, &dri2Minor)) {
    GST_ERROR ("!!Error : DRI2QueryVersion !!");
    return NULL;
  }
  if (!DRI2Connect (dpy, RootWindow(dpy, screen), &driverName, &deviceName)) {
    GST_ERROR ( "!!Error : DRI2Connect !!");
    if(driverName) Xfree(driverName);
    if(deviceName) Xfree(deviceName);
    return NULL;
  }
  if(driverName) Xfree(driverName);
  GST_DEBUG("Open drm device : %s", deviceName);
  // get the drm_fd though opening the deviceName
  drm_fd = open (deviceName, O_RDWR);
  if (drm_fd < 0) {
    GST_ERROR ("!!Error : cannot open drm device (%s)", deviceName);
    if(deviceName) Xfree(deviceName);
    return NULL;
  }
  if(deviceName) Xfree(deviceName);
  /* get the drm magic */
  drmGetMagic(drm_fd, &magic);
  fprintf(stderr, ">>> drm magic=%d \n", magic);
  if (!DRI2Authenticate(dpy, RootWindow(dpy, screen), magic))
  {
    fprintf(stderr, "!!Error : DRI2Authenticate !!\n");
    close (drm_fd);
    return NULL;
  }
  // drm slp buffer manager init
  bufmgr = tbm_bufmgr_init (drm_fd);
  if (!bufmgr) {
    GST_ERROR ("!!Error : fail to init buffer manager ");
    close (drm_fd);
    return NULL;
  }
  DRI2CreateDrawable (dpy, pixmap);
  return bufmgr;
}

static int port_get (Display *dpy, unsigned int id)
{
  unsigned int ver, rev, req_base, evt_base, err_base;
  unsigned int adaptors;
  XvAdaptorInfo *ai = NULL;
  XvAttribute *at = NULL;
  XvImageFormatValues *fo = NULL;
  int attributes, formats;
  int i, j, p;

  if (XvQueryExtension (dpy, &ver, &rev, &req_base, &evt_base, &err_base) != Success) return -1;
  if (XvQueryAdaptors (dpy, DefaultRootWindow (dpy), &adaptors, &ai) != Success) return -1;
  if (!ai) return -1;
  for (i = 0; i < adaptors; i++) {
    int support_format = False;
    if (!(ai[i].type & XvStillMask)) continue;
    GST_LOG ("===========================================");
    GST_LOG (" name:        %s"
                " first port:  %ld"
                " ports:       %ld",
                ai[i].name,
                ai[i].base_id,
                ai[i].num_ports);
    p = ai[i].base_id;
    GST_LOG (" attribute list:");
    at = XvQueryPortAttributes (dpy, p, &attributes);
    for (j = 0; j < attributes; j++) GST_LOG ("\t-  name: %s\n"
                    "\t\t  flags:     %s%s\n"
                    "\t\t  min_value:  %i\n"
                    "\t\t  max_value:  %i\n",
                    at[j].name,
                    (at[j].flags & XvGettable) ? " get" : "",
                    (at[j].flags & XvSettable) ? " set" : "",
                    at[j].min_value, at[j].max_value);
    if (at) XFree (at);
    GST_LOG (" image format list:");
    fo = XvListImageFormats (dpy, p, &formats);
    for (j = 0; j < formats; j++) {
      GST_LOG ("\t-  0x%x (%4.4s) %s", fo[j].id, (char *)&fo[j].id, (fo[j].format == XvPacked) ? "packed" : "planar");
      if (fo[j].id == (int)id) support_format = True;
    }
    if (fo) XFree (fo);
    if (!support_format) continue;
    for (; p < ai[i].base_id + ai[i].num_ports; p++) {
      if (XvGrabPort (dpy, p, 0) == Success) {
        GST_LOG ("========================================");
        GST_DEBUG ("XvGrabPort success : %d", p);
        GST_LOG ("========================================");
        XvFreeAdaptorInfo (ai);
        return p;
      }
    }
  }
  XvFreeAdaptorInfo (ai);
  return -1;
}

static void pixmap_update (GstXVImageSrc * src, Display *dpy, tbm_bufmgr bufmgr, Pixmap pixmap,
               int x, int y, int width, int height)
{
  unsigned int attachments[1];
  int dri2_count, dri2_out_count;
  int dri2_width, dri2_height, dri2_stride;
  int opt;
  tbm_bo_handle temp_virtual;
  attachments[0] = DRI2BufferFrontLeft;
  dri2_count = 1;
  src->dri2_buffers = DRI2GetBuffers (dpy, pixmap, &dri2_width, &dri2_height, attachments, dri2_count, &dri2_out_count);
  if (!src->dri2_buffers) {
    GST_ERROR ("[Error] : fail to get buffers");
    goto update_done;
  }
  if (!src->dri2_buffers[0].name) {
    GST_ERROR ("[Error] : a handle of the dri2 buffer is null  ");
    goto update_done;
  }
  src->bo = tbm_bo_import(bufmgr, src->dri2_buffers[0].name);
  if (!src->bo) {
    GST_ERROR ("[Error] : cannot import bo (key:%d)", src->dri2_buffers[0].name);
    goto update_done;
  }
  dri2_stride = src->dri2_buffers[0].pitch;
  opt = TBM_OPTION_READ|TBM_OPTION_WRITE;
  temp_virtual = tbm_bo_map (src->bo, TBM_DEVICE_CPU, opt);
  src->virtual = temp_virtual.ptr;
  if (!src->virtual) {
    GST_ERROR ("[Error] : fail to map ");
    goto update_done;
  }
  return;
update_done:
  if (src->virtual) tbm_bo_unmap(src->bo);
  src->virtual = NULL;
  if (src->bo) tbm_bo_unref(src->bo);
  src->bo = NULL;
  if (src->dri2_buffers) free(src->dri2_buffers);
  src->dri2_buffers = NULL;
  return;
}

static Pixmap pixmap_create (GstXVImageSrc * src, Display *dpy, int width, int height)
{
  src->pixmap = XCreatePixmap(dpy, DefaultRootWindow(dpy), width, height, DefaultDepth(dpy, DefaultScreen(dpy)));
  src->gc = XCreateGC (dpy, src->pixmap, 0, 0);
  XSetForeground (dpy, src->gc, 0xFFFF0000);
  XFillRectangle (dpy, src->pixmap, src->gc, 0, 0, width, height);
  XSync(dpy, 0);
  XFreeGC (dpy, src->gc);
  return src->pixmap;
}

static gboolean
gst_xv_image_src_stop (GstBaseSrc * bsrc)
{
  GstXVImageSrc *src = GST_XV_IMAGE_SRC (bsrc);
  GST_DEBUG_OBJECT (src, "stop()");

  if(src->updates_thread) {
    src->thread_return = TRUE;
    g_thread_join ( src->updates_thread);
    src->updates_thread = NULL;
  }
  GST_DEBUG_OBJECT (src, "stop end ");
  return TRUE;
}

/* Interrupt a blocking request. */
static gboolean
gst_xv_image_src_unlock (GstBaseSrc * bsrc)
{
  GstXVImageSrc *src = GST_XV_IMAGE_SRC (bsrc);
  GST_DEBUG_OBJECT (src, "unlock()");
  return TRUE;
}

/* Interrupt interrupt. */
static gboolean
gst_xv_image_src_unlock_stop (GstBaseSrc * bsrc)
{
  GstXVImageSrc *src = GST_XV_IMAGE_SRC (bsrc);
  GST_DEBUG_OBJECT (src, "unlock_stop()");
  return TRUE;
}

static gboolean
gst_xv_image_src_get_size (GstBaseSrc * bsrc, guint64 * size)
{
  GstXVImageSrc *src = GST_XV_IMAGE_SRC (bsrc);
  GST_INFO ("Get size %x", src);
  return FALSE;
}

static gboolean
gst_xv_image_src_is_seekable (GstBaseSrc * bsrc)
{
  GstXVImageSrc *src = GST_XV_IMAGE_SRC (bsrc);
  GST_INFO ("Is seekable %x", src);
  return FALSE;
}

static gboolean
gst_xv_image_src_query (GstBaseSrc * bsrc, GstQuery * query)
{
  GstXVImageSrc *src = GST_XV_IMAGE_SRC (bsrc);
  gboolean ret;
  GST_INFO ("src query %x", src);
  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_URI:
      ret = TRUE;
    break;
    default:
      ret = FALSE;
    break;
  }

  if (!ret)
    ret = GST_BASE_SRC_CLASS (parent_class)->query (bsrc, query);

  return ret;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (xvimagesrc_debug, "xvimagesrc", 0, "XServer display FB video capture Source");
  return gst_element_register (plugin, "xvimagesrc", GST_RANK_PRIMARY, GST_TYPE_XV_IMAGE_SRC);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
  GST_VERSION_MINOR,
  "xvimage",
  "XServer display video src",
  plugin_init, VERSION, "LGPL", "Samsung Electronics Co", "http://www.samsung.com")
