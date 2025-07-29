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

/* helper function */
static void print_surface_debug_info(const NvBufSurface *surface) {
    g_print("Batch size: %u\n", surface->batchSize);
    g_print("memory type: %d\n", surface->memType);

    g_print("Frame size: %dX%d\n", surface->surfaceList[0].width, surface->surfaceList[0].height);
    g_print("Frame pitch: %d\n", surface->surfaceList[0].pitch);
    g_print("Frame color format: %d\n", surface->surfaceList[0].colorFormat);
    g_print("Frame data size: %d\n", surface->surfaceList[0].dataSize);
    g_print("Number of planes: %d\n", surface->surfaceList[0].planeParams.num_planes);
}

static void save_y_plane_from_surface_to_host(NvBufSurface *surface) {
    if (!surface || surface->numFilled == 0) {
        g_print("No valid surface data to save.\n");
        return;
    }

    g_print("Saving frame from NvBufSurface...\n");
    print_surface_debug_info(surface);

    NvBufSurfaceMap(surface, 0, 0, NVBUF_MAP_READ);
    NvBufSurfaceSyncForCpu(surface, 0, 0);

    int width = surface->surfaceList[0].width;
    int height = surface->surfaceList[0].height;
    int pitch = surface->surfaceList[0].pitch;
    uchar *y_cpu_ptr = (uchar*)surface->surfaceList[0].mappedAddr.addr[0];

    cv::Mat y_cpu = cv::Mat(height, width, CV_8UC1, y_cpu_ptr, pitch);
    
    std::string y_path = "./frame/y.png";
    cv::imwrite(y_path, y_cpu);

    NvBufSurfaceUnMap(surface, 0, 0);

}

/* transform function */
static GstFlowReturn
gst_video_stabilizer_transform(GstBaseTransform *base, GstBuffer *inbuf, GstBuffer *outbuf) {
  
  GstVideoStabilizer *self = (GstVideoStabilizer *)base;

  GstMapInfo in_map, out_map;
  gst_buffer_map(inbuf, &in_map, GST_MAP_READ);
  gst_buffer_map(outbuf, &out_map, GST_MAP_WRITE);

  
  NvBufSurface *in_surface = (NvBufSurface*)in_map.data;

  auto frame = in_surface->surfaceList[0].dataPtr;
  
  if (!frame) {
      g_print("Failed to get frame data from NvBufSurface\n");

      gst_buffer_unmap(inbuf, &in_map);
      gst_buffer_unmap(outbuf, &out_map);
      return GST_FLOW_ERROR;
    }
    
  int width = in_surface->surfaceList[0].width;
  int height = in_surface->surfaceList[0].height;
  int pitch = in_surface->surfaceList[0].pitch;
  uchar *y_d_ptr = (uchar*)in_surface->surfaceList[0].dataPtr;

  cv::cuda::GpuMat d_gray(height, width, CV_8UC1, y_d_ptr, pitch);

  if (self->first_frame) {
    g_print("Processing first frame...\n");
    //FIXME: initilize prev_gray before use.
    // d_gray.copyTo(self->prev_gray);
    self->first_frame = FALSE;
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
  
  /* Per-instance state */
  filter->first_frame = TRUE;
  filter->processed_frame_count = 0;

  /* Create the CUDA TV-L1 solver */
  filter->tvl1 = cv::cuda::OpticalFlowDual_TVL1::create();

  /* Start with an identity 2×3 affine transform */
  filter->last_transform = cv::Mat::eye(2, 3, CV_32F);

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
