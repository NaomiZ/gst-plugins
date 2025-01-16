#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/base/gstaggregator.h>
#include <nvdsmeta.h>
#include "gstanalyticsaggregator.h"

#define PLUGIN_NAME "analyticsaggregator"

G_DEFINE_TYPE(GstAnalyticsAggregator, gst_analytics_aggregator, GST_TYPE_AGGREGATOR);

static GstCaps *gst_analytics_aggregator_get_caps(GstAggregator *agg, GstPad *pad, GstCaps *filter);
static gboolean gst_analytics_aggregator_sink_event(GstAggregator *agg, GstAggregatorPad *pad, GstEvent *event);
static GstFlowReturn gst_analytics_aggregator_aggregate(GstAggregator *agg, gboolean timeout);

#define GST_CAPS_FEATURE_MEMORY_NVMM "memory:NVMM"

static GstStaticPadTemplate gst_analytics_aggregator_video_sink_template =
    GST_STATIC_PAD_TEMPLATE ("video_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_NVMM,
            "{ NV12, RGBA, I420 }")));

static GstStaticPadTemplate gst_analytics_aggregator_meta_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_NVMM,
            "{ NV12, RGBA, I420 }")));

static GstStaticPadTemplate gst_analytics_aggregator_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_NVMM,
            "{ NV12, RGBA, I420 }")));

static void gst_analytics_aggregator_class_init(GstAnalyticsAggregatorClass *klass) {

    
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    GstAggregatorClass *aggregator_class = GST_AGGREGATOR_CLASS(klass);

    gst_element_class_set_static_metadata(element_class,
        "Analytics Aggregator",
        "Aggregator/Metadata",
        "Aggregates metadata from secondary streams to a primary video stream",
        "Your Name <your.email@example.com>");
    
    aggregator_class->sink_event = GST_DEBUG_FUNCPTR(gst_analytics_aggregator_sink_event);
    aggregator_class->aggregate = GST_DEBUG_FUNCPTR(gst_analytics_aggregator_aggregate);
    
    // Add pad templates for the video sink, dynamic sink, and source pads
    gst_element_class_add_static_pad_template(element_class, &gst_analytics_aggregator_video_sink_template);
    gst_element_class_add_static_pad_template(element_class, &gst_analytics_aggregator_meta_sink_template);
    gst_element_class_add_static_pad_template(element_class, &gst_analytics_aggregator_src_template);
    g_print("gst_analytics_aggregator_class_init\n");
}

static void gst_analytics_aggregator_init(GstAnalyticsAggregator *self) {
    g_print("gst_analytics_aggregator_init\n");
    self->video_sink_pad = gst_pad_new_from_static_template(&gst_analytics_aggregator_video_sink_template, "video_sink");
    gst_element_add_pad(GST_ELEMENT(self), self->video_sink_pad);

    self->src_pad = gst_pad_new_from_static_template(&gst_analytics_aggregator_src_template, "src");
    //GstAggrigator (the element parent) has already added the src pad
    
    self->dynamic_sink_pads = NULL;
}

static gboolean gst_analytics_aggregator_sink_event(GstAggregator *agg, GstAggregatorPad *pad, GstEvent *event) {
    // Implement event handling logic here
    return GST_AGGREGATOR_CLASS(gst_analytics_aggregator_parent_class)->sink_event(agg, pad, event);
}

static GstFlowReturn gst_analytics_aggregator_aggregate(GstAggregator *agg, gboolean timeout) {
    // Implement aggregation logic here
    return GST_FLOW_OK;
}

static gboolean plugin_init(GstPlugin *plugin) {
    return gst_element_register(plugin, PLUGIN_NAME, GST_RANK_NONE, GST_TYPE_ANALYTICS_AGGREGATOR);
}

#define PACKAGE "analyticsaggregator"
GST_PLUGIN_DEFINE(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    analyticsaggregator,
    "Analytics Aggregator Plugin",
    plugin_init,
    "1.0",
    "LGPL",
    "GStreamer",
    "https://gstreamer.freedesktop.org/")