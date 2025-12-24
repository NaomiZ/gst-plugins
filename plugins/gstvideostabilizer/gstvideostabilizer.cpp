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

bool NvBufSurfaceToGpuMat(NvBufSurface* surf, int batch_index, cv::cuda::GpuMat &output) {
    cudaError_t err;
    cudaGraphicsResource *cudaResource = nullptr;

    // 1. Map the NvBufSurface to EGLImage
    if (NvBufSurfaceMapEglImage(surf, batch_index) != 0) {
        fprintf(stderr, "Failed to map EGLImage\n");
        return false;
    }
    EGLImageKHR eglImage = surf->surfaceList[batch_index].mappedAddr.eglImage;

    // 2. Register EGLImage with CUDA
    err = cudaGraphicsEGLRegisterImage(&cudaResource, eglImage, cudaGraphicsRegisterFlagsReadOnly);
    if (err != cudaSuccess) {
        fprintf(stderr, "cudaGraphicsEGLRegisterImage failed: %s\n", cudaGetErrorString(err));
        NvBufSurfaceUnMapEglImage(surf, batch_index);
        return false;
    }

    // 3. Map CUDA resource
    err = cudaGraphicsMapResources(1, &cudaResource, 0);
    if (err != cudaSuccess) {
        fprintf(stderr, "cudaGraphicsMapResources failed: %s\n", cudaGetErrorString(err));
        cudaGraphicsUnregisterResource(cudaResource);
        NvBufSurfaceUnMapEglImage(surf, batch_index);
        return false;
    }

    // 4. Get cudaArray from resource
    cudaArray_t cudaArray;
    err = cudaGraphicsSubResourceGetMappedArray(&cudaArray, cudaResource, 0, 0);
    if (err != cudaSuccess) {
        fprintf(stderr, "cudaGraphicsSubResourceGetMappedArray failed: %s\n", cudaGetErrorString(err));
        cudaGraphicsUnmapResources(1, &cudaResource, 0);
        cudaGraphicsUnregisterResource(cudaResource);
        NvBufSurfaceUnMapEglImage(surf, batch_index);
        return false;
    }

    // 5. Allocate output GpuMat (Y plane only, assuming NV12)
    int width  = surf->surfaceList[batch_index].width;
    int height = surf->surfaceList[batch_index].height;
    output.create(height, width, CV_8UC1);

    // 6. Copy from cudaArray to GpuMat
    err = cudaMemcpy2DFromArray(
        output.data, output.step,
        cudaArray, 0, 0,
        width, height,
        cudaMemcpyDeviceToDevice
    );
    if (err != cudaSuccess) {
        fprintf(stderr, "cudaMemcpy2DFromArray failed: %s\n", cudaGetErrorString(err));
        cudaGraphicsUnmapResources(1, &cudaResource, 0);
        cudaGraphicsUnregisterResource(cudaResource);
        NvBufSurfaceUnMapEglImage(surf, batch_index);
        return false;
    }

    // 7. Cleanup
    cudaGraphicsUnmapResources(1, &cudaResource, 0);
    cudaGraphicsUnregisterResource(cudaResource);
    NvBufSurfaceUnMapEglImage(surf, batch_index);

    return true;
}

/* transform function */
static GstFlowReturn
gst_video_stabilizer_transform(GstBaseTransform *base, GstBuffer *inbuf, GstBuffer *outbuf) {
  
  GstVideoStabilizer *self = (GstVideoStabilizer *)base;

  GstMapInfo in_map, out_map;
  gst_buffer_map(inbuf, &in_map, GST_MAP_READ);
  gst_buffer_map(outbuf, &out_map, GST_MAP_WRITE);

  
  NvBufSurface *in_surface = (NvBufSurface*)in_map.data;
  NvBufSurfaceMap(in_surface, -1, -1, NVBUF_MAP_READ);
  NvBufSurfaceSyncForDevice(in_surface, -1, -1);

  //TODO: validate in_surface->surfaceList[0]
  uint8_t* frame = in_surface->surfaceList[0].dataPtr;
  uint32_t width = in_surface->surfaceList[0].width;
  uint32_t height = in_surface->surfaceList[0].height;
  uint32_t pitch = in_surface->surfaceList[0].pitch;
  uint32_t frame_size = in_surface->surfaceList[0].dataSize;

  if (!frame) {
    g_print("Failed to get frame data from NvBufSurface\n");

    gst_buffer_unmap(inbuf, &in_map);
    gst_buffer_unmap(outbuf, &out_map);
    return GST_FLOW_ERROR;
  }
  cv::cuda::GpuMat d_gray;

  if (self->first_frame) {
    // You already have almost identical function save_y_plane_from_surface_to_host()
    g_print("Processing first frame...\n");    
    cv::Mat h_frame;
    d_gray.download(h_frame);
    cv::imwrite("./frame/h.png", h_frame);
    self->first_frame = FALSE;
  }
  
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
