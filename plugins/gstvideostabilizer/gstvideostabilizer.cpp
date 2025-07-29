#include "gstvideostabilizer.h"
#include <gst/gst.h>
#include <string.h>
#include <opencv2/core.hpp>
#include <opencv2/cudaarithm.hpp>
#include <opencv2/imgcodecs.hpp>
#include <nvbufsurface.h>

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
    gst_buffer_peek_memory(inbuf, 0);
    NvBufSurface *in_surface = (NvBufSurface*)in_map.data;

    NvBufSurfaceMap(in_surface, -1, -1, NVBUF_MAP_READ);
    NvBufSurfaceSyncForCpu(in_surface, -1, -1);

    auto frame = in_surface->surfaceList[0].dataPtr;
    
    if (!frame) {
        g_print("Failed to get frame data from NvBufSurface\n");
            g_print("Failed to get frame data from NvBufSurface\n");NvBufSurfaceUnMap(in_surface, -1, -1);

        gst_buffer_unmap(inbuf, &in_map);
        gst_buffer_unmap(outbuf, &out_map);
        return GST_FLOW_ERROR;
    }

    g_print("Batch size: %u\n", in_surface->batchSize);
    g_print("memory type: %d\n", in_surface->memType);

    g_print("Frame size: %dX%d\n", in_surface->surfaceList[0].width, in_surface->surfaceList[0].height);
    g_print("Frame pitch: %d\n", in_surface->surfaceList[0].pitch);
    g_print("Frame color format: %d\n", in_surface->surfaceList[0].colorFormat);
    g_print("Frame data size: %d\n", in_surface->surfaceList[0].dataSize);
    g_print("Number of planes: %d\n", in_surface->surfaceList[0].planeParams.num_planes);

    int width = in_surface->surfaceList[0].width;
    int height = in_surface->surfaceList[0].height;
    int pitch = in_surface->surfaceList[0].pitch;
    uchar *y_ptr = (uchar*)in_surface->surfaceList[0].mappedAddr.addr[0];

    cv::Mat y_cpu = cv::Mat(height, width, CV_8UC1, y_ptr, pitch);
    
    cv::Mat y_contig;
    y_cpu.copyTo(y_contig);
    
    std::string y_path = "./frame/y.png";
    cv::imwrite(y_path, y_contig);
    
    memcpy(out_map.data, in_map.data, in_map.size);

    NvBufSurfaceUnMap(in_surface, -1, -1);
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
