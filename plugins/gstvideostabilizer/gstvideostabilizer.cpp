#include "gstvideostabilizer.h"
#include <gst/gst.h>
#include <string.h>
#include <opencv2/core.hpp>
#include <opencv2/cudaarithm.hpp>

/* static pad templates */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
  "sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS("video/x-raw(memory:NVMM), format=(string)NV12")
);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
  "src",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS("video/x-raw(memory:NVMM), format=(string)NV12")
);

/* class definition */
G_DEFINE_TYPE(GstVideoStabilizer, gst_video_stabilizer, GST_TYPE_BASE_TRANSFORM);

/* transform function */
static GstFlowReturn
gst_video_stabilizer_transform(GstBaseTransform *base, GstBuffer *inbuf, GstBuffer *outbuf) {
    GstMapInfo in_map, out_map;
    gst_buffer_map(inbuf, &in_map, GST_MAP_READ);
    gst_buffer_map(outbuf, &out_map, GST_MAP_WRITE);
    g_print("Processing buffer of size: %zu\n", in_map.size);
    // Load the frame to cv::cuda::GpuMat using OpenCV with CUDA
    // Assume frame is NV12, width/height are known or can be queried from caps
    GstCaps *caps = gst_pad_get_current_caps(GST_BASE_TRANSFORM_SRC_PAD(base));
    int width = 0, height = 0;
    if (caps) {
        GstStructure *caps_structure = gst_caps_get_structure(caps, 0);
        gst_structure_get_int(caps_structure, "width", &width);
        gst_structure_get_int(caps_structure, "height", &height);
        gst_caps_unref(caps);
    }

    if (width > 0 && height > 0) {
        // NV12: Y plane (width*height), UV plane (width*height/2)
        cv::cuda::GpuMat gpu_y(height, width, CV_8UC1, (void*)in_map.data);
        cv::cuda::GpuMat gpu_uv(height/2, width, CV_8UC1, (void*)(in_map.data + width*height));
        // Now gpu_y and gpu_uv hold the Y and UV planes on GPU
        // You can process them as needed
    }
    memcpy(out_map.data, in_map.data, in_map.size);
    gst_buffer_unmap(inbuf, &in_map);
    gst_buffer_unmap(outbuf, &out_map);
    return GST_FLOW_OK;
}

/* class init */
static void
gst_video_stabilizer_class_init(GstVideoStabilizerClass *klass) {
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

  gst_element_class_add_static_pad_template(element_class, &sink_template);
  gst_element_class_add_static_pad_template(element_class, &src_template);

  gst_element_class_set_static_metadata(element_class,
    "Video Stabilizer", "Filter/Effect/Video",
    "A basic video stabilizer template","Your Name <you@example.com>");

  GstBaseTransformClass *base_class = GST_BASE_TRANSFORM_CLASS(klass);
  base_class->transform = GST_DEBUG_FUNCPTR(gst_video_stabilizer_transform);
}

/* instance init */
static void
gst_video_stabilizer_init(GstVideoStabilizer *filter) {
  /* not in-place, separate input/output buffers */
  gst_base_transform_set_in_place(GST_BASE_TRANSFORM(filter), FALSE);
}

/* plugin init */
static gboolean
gst_video_stabilizer_plugin_init(GstPlugin *plugin) {
  return gst_element_register(plugin, "videostabilizer", GST_RANK_NONE, GST_TYPE_VIDEO_STABILIZER);
}

GST_PLUGIN_DEFINE(
  GST_VERSION_MAJOR,
  GST_VERSION_MINOR,
  videostabilizer,
  "Video Stabilizer Plugin",
  gst_video_stabilizer_plugin_init,
  "1.0",
  "LGPL",
  "GStreamer",
  "http://gstreamer.net/"
)
