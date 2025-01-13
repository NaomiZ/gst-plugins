#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/base/gstaggregator.h>
#include <nvdsmeta.h>

#define PLUGIN_NAME "analyticsaggregator"

typedef struct _GstAnalyticsAggregator {
    GstAggregator parent;
    GstPad *video_sink_pad;
    GList *dynamic_sink_pads;
    GstPad *src_pad;
} GstAnalyticsAggregator;

typedef struct _GstAnalyticsAggregatorClass {
    GstAggregatorClass parent_class;
} GstAnalyticsAggregatorClass;

G_DEFINE_TYPE(GstAnalyticsAggregator, gst_analytics_aggregator, GST_TYPE_AGGREGATOR);

static GstCaps *gst_analytics_aggregator_get_caps(GstAggregator *agg, GstPad *pad, GstCaps *filter);
static gboolean gst_analytics_aggregator_sink_event(GstAggregator *agg, GstAggregatorPad *pad, GstEvent *event);
static GstFlowReturn gst_analytics_aggregator_aggregate(GstAggregator *agg, gboolean timeout);

static void gst_analytics_aggregator_class_init(GstAnalyticsAggregatorClass *klass) {
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    GstAggregatorClass *aggregator_class = GST_AGGREGATOR_CLASS(klass);

    gst_element_class_set_static_metadata(element_class,
        "Analytics Aggregator",
        "Aggregator/Metadata",
        "Aggregates metadata from secondary streams to a primary video stream",
        "Your Name <your.email@example.com>");

    gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get(&gst_static_pad_template_factory(
            "video_sink", GST_PAD_SINK, GST_PAD_ALWAYS,
            gst_caps_from_string("video/x-raw(memory:NVMM), format=(string){NV12,RGBA}, width=[1,2147483647], height=[1,2147483647], framerate=[0/1,2147483647/1]"))));

    gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get(&gst_static_pad_template_factory(
            "sink_%u", GST_PAD_SINK, GST_PAD_REQUEST,
            gst_caps_from_string("video/x-raw(memory:NVMM), format=(string){NV12,RGBA}, width=[1,2147483647], height=[1,2147483647], framerate=[0/1,2147483647/1]"))));

    gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get(&gst_static_pad_template_factory(
            "src", GST_PAD_SRC, GST_PAD_ALWAYS,
            gst_caps_from_string("video/x-raw(memory:NVMM), format=(string){NV12,RGBA}, width=[1,2147483647], height=[1,2147483647], framerate=[0/1,2147483647/1]"))));

    aggregator_class->get_caps = GST_DEBUG_FUNCPTR(gst_analytics_aggregator_get_caps);
    aggregator_class->sink_event = GST_DEBUG_FUNCPTR(gst_analytics_aggregator_sink_event);
    aggregator_class->aggregate = GST_DEBUG_FUNCPTR(gst_analytics_aggregator_aggregate);
}

static void gst_analytics_aggregator_init(GstAnalyticsAggregator *self) {
    self->video_sink_pad = gst_aggregator_get_sink_pad(GST_AGGREGATOR(self), "video_sink");
    self->src_pad = gst_aggregator_get_src_pad(GST_AGGREGATOR(self));
    self->dynamic_sink_pads = NULL;
}

static GstCaps *gst_analytics_aggregator_get_caps(GstAggregator *agg, GstPad *pad, GstCaps *filter) {
    // Implement format validation logic here
    return GST_PAD_CAPS(pad);
}

static gboolean gst_analytics_aggregator_sink_event(GstAggregator *agg, GstAggregatorPad *pad, GstEvent *event) {
    // Implement event handling logic here
    return GST_AGGREGATOR_CLASS(gst_analytics_aggregator_parent_class)->sink_event(agg, pad, event);
}

static GstFlowReturn gst_analytics_aggregator_aggregate(GstAggregator *agg, gboolean timeout) {
    GstAnalyticsAggregator *self = GST_ANALYTICS_AGGREGATOR(agg);
    GstBuffer *video_buffer = gst_aggregator_pad_get_buffer(GST_AGGREGATOR_PAD(self->video_sink_pad));
    if (!video_buffer) {
        return GST_FLOW_ERROR;
    }

    // Implement metadata extraction and merging logic here

    return gst_aggregator_finish_buffer(agg, video_buffer);
}

static gboolean plugin_init(GstPlugin *plugin) {
    return gst_element_register(plugin, PLUGIN_NAME, GST_RANK_NONE, GST_TYPE_ANALYTICS_AGGREGATOR);
}

GST_PLUGIN_DEFINE(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    analyticsaggregator,
    "Analytics Aggregator Plugin",
    plugin_init,
    "1.0",
    "LGPL",
    "GStreamer",
    "https://gstreamer.freedesktop.org/"
)