#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/base/gstaggregator.h>
#include <nvdsmeta.h>
#include <gstnvdsmeta.h>
#include "gstanalyticsaggregator.h"
#include <stdio.h>

#define PLUGIN_NAME "analyticsaggregator"

G_DEFINE_TYPE(GstAnalyticsAggregator, gst_analytics_aggregator, GST_TYPE_AGGREGATOR);

static GstFlowReturn gst_analytics_aggregator_update_src_caps(GstAggregator *aggregator, GstCaps *caps, GstCaps **ret);
static gboolean gst_analytics_aggregator_sink_query(GstAggregator *parent, GstAggregatorPad *pad, GstQuery *query);
static GstAggregatorPad *gst_analytics_aggregator_create_new_pad(GstAggregator *agg, GstPadTemplate *templ, const gchar *name, const GstCaps *caps);
static gboolean gst_analytics_aggregator_sink_event(GstAggregator *agg, GstAggregatorPad *pad, GstEvent *event);
static GstFlowReturn gst_analytics_aggregator_aggregate(GstAggregator *agg, gboolean timeout);

#define GST_CAPS_FEATURE_MEMORY_NVMM "memory:NVMM"

static GstStaticPadTemplate video_sink_factory =
    GST_STATIC_PAD_TEMPLATE ("video_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_NVMM,
            "{ NV12, RGBA, I420 }")));

static GstStaticPadTemplate meta_sink_factory =
    GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_NVMM,
            "{ NV12, RGBA, I420 }")));

static GstStaticPadTemplate src_factory =
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
    aggregator_class->create_new_pad = GST_DEBUG_FUNCPTR(gst_analytics_aggregator_create_new_pad);
    aggregator_class->sink_query = GST_DEBUG_FUNCPTR(gst_analytics_aggregator_sink_query);
    aggregator_class->update_src_caps = GST_DEBUG_FUNCPTR(gst_analytics_aggregator_update_src_caps);
    
    // Add pad templates for the video sink, dynamic sink, and source pads
    gst_element_class_add_static_pad_template_with_gtype(element_class, &video_sink_factory, GST_TYPE_AGGREGATOR_PAD);
    gst_element_class_add_static_pad_template_with_gtype(element_class, &meta_sink_factory, GST_TYPE_AGGREGATOR_PAD);
    gst_element_class_add_static_pad_template_with_gtype(element_class, &src_factory, GST_TYPE_AGGREGATOR_PAD);
}

static void gst_analytics_aggregator_init(GstAnalyticsAggregator *self) {
    self->video_sink_pad = GST_AGGREGATOR_PAD(g_object_new(GST_TYPE_AGGREGATOR_PAD,
                                        "name", "video_sink",
                                        "direction", GST_PAD_SINK, NULL));
    gst_element_add_pad(GST_ELEMENT(self), GST_PAD(self->video_sink_pad));

    self->dynamic_sink_pads = NULL;
}

static gboolean gst_analytics_aggregator_sink_event(GstAggregator *agg, GstAggregatorPad *pad, GstEvent *event) {
    // Implement event handling logic here
    switch (GST_EVENT_TYPE(event)) {
        case GST_EVENT_EOS:
            // Handle end-of-stream event
            GST_DEBUG_OBJECT(agg, "Received EOS event");
            break;
        case GST_EVENT_FLUSH_START:
            // Handle flush start event
            GST_DEBUG_OBJECT(agg, "Received FLUSH_START event");
            break;
        case GST_EVENT_FLUSH_STOP:
            // Handle flush stop event
            GST_DEBUG_OBJECT(agg, "Received FLUSH_STOP event");
            break;
        default:
            // Pass other events to the default handler
            break;
    }
    return GST_AGGREGATOR_CLASS(gst_analytics_aggregator_parent_class)->sink_event(agg, pad, event);
}

static GstFlowReturn gst_analytics_aggregator_aggregate(GstAggregator *agg, gboolean timeout) {
    GstBuffer *outbuf = NULL;
    GstBuffer *inbuf = NULL;
    GstAggregatorPad *pad;
    GstClockTime pts = GST_CLOCK_TIME_NONE;
    GstClockTime dts = GST_CLOCK_TIME_NONE;
    GstClockTime duration = GST_CLOCK_TIME_NONE;    

    GstAnalyticsAggregator *self = GST_ANALYTICS_AGGREGATOR(agg);
    outbuf = gst_aggregator_pad_pop_buffer(GST_AGGREGATOR_PAD(self->video_sink_pad));
    if (!outbuf) {
        GST_ERROR_OBJECT(self, "Failed to pop buffer from video sink pad");
        return GST_FLOW_ERROR;
    }

    pts = GST_BUFFER_PTS(outbuf);
    dts = GST_BUFFER_DTS(outbuf);
    duration = GST_BUFFER_DURATION(outbuf);

    GList *walk;

    for (walk = self->dynamic_sink_pads; walk; walk = g_list_next(walk)) {
        pad = GST_AGGREGATOR_PAD(walk->data);
        if (pad) {
            inbuf = gst_aggregator_pad_pop_buffer(pad);
            if (inbuf) {
                NvDsMetaList *l;
                NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta(inbuf);
                if (batch_meta) {
                    for (l = batch_meta->frame_meta_list; l != NULL; l = l->next) {
                        NvDsFrameMeta *frame_meta = (NvDsFrameMeta *)(l->data);
                        if (frame_meta) {
                            NvDsUserMetaList *user_meta_list = frame_meta->frame_user_meta_list;
                            while (user_meta_list) {
                                NvDsUserMeta *user_meta = (NvDsUserMeta *)(user_meta_list->data);
                                if (user_meta) {
                                    gst_buffer_add_nvds_meta(outbuf, user_meta, NULL, NULL, NULL);
                                }
                                user_meta_list = user_meta_list->next;
                            }
                        }
                    }
                }
                gst_buffer_unref(inbuf);
            }
        }
        else {
            GST_WARNING_OBJECT(self, "Pad is null: %s, removing pad", GST_PAD_NAME(pad));
            self->dynamic_sink_pads = g_list_remove(self->dynamic_sink_pads, pad);
        }
    }

    gst_aggregator_selected_samples(agg, pts, dts, duration, NULL);
    gst_aggregator_finish_buffer(GST_AGGREGATOR(self), outbuf);
    return GST_FLOW_OK;
    return GST_FLOW_OK;
}

GstFlowReturn gst_analytics_aggregator_update_src_caps(GstAggregator *aggregator, GstCaps *caps, GstCaps **ret)
{
    GstAnalyticsAggregator *self = GST_ANALYTICS_AGGREGATOR(aggregator);
    GstCaps *video_sink_caps = gst_pad_get_current_caps(GST_PAD(self->video_sink_pad));
    if (!video_sink_caps || gst_caps_is_empty(video_sink_caps)) {
        GST_WARNING_OBJECT(aggregator, "Video sink pad caps are not set, using default caps");
        video_sink_caps = gst_caps_new_simple("video/x-raw",
                                            "format", G_TYPE_STRING, "NV12",
                                            "width", G_TYPE_INT, 320,
                                            "height", G_TYPE_INT, 240,
                                            "framerate", GST_TYPE_FRACTION, 30, 1,
                                            NULL);
    }

    if (!gst_caps_can_intersect(video_sink_caps, caps)) {
        GST_ERROR_OBJECT(aggregator, "Caps are not compatible with video sink pad caps");
        gst_caps_unref(video_sink_caps);
        return GST_FLOW_ERROR;
    }

    *ret = gst_caps_copy(video_sink_caps);
    gst_caps_unref(video_sink_caps);
    return GST_FLOW_OK;
}

static gboolean gst_analytics_aggregator_sink_query(GstAggregator *parent, GstAggregatorPad *pad, GstQuery *query)
{
    gboolean result = FALSE;

    switch (GST_QUERY_TYPE(query)) {
        case GST_QUERY_CAPS:
        {
            GstCaps *filter;
            gst_query_parse_caps(query, &filter);
            GstCaps *caps = gst_pad_get_pad_template_caps(GST_PAD(pad));
            if (filter) {
                GstCaps *intersection = gst_caps_intersect_full(filter, caps, GST_CAPS_INTERSECT_FIRST);
                gst_caps_unref(caps);
                caps = intersection;
            }
            gst_query_set_caps_result(query, caps);
            gst_caps_unref(caps);
            result = TRUE;
            break;
        }
        default:
            result = GST_AGGREGATOR_CLASS(gst_analytics_aggregator_parent_class)->sink_query(GST_AGGREGATOR(parent), pad, query);
            break;
    }

    return result;
}

static GstAggregatorPad *gst_analytics_aggregator_create_new_pad(GstAggregator *agg, GstPadTemplate *templ, const gchar *name, const GstCaps *caps)
{
    GstElementClass *klass = GST_ELEMENT_GET_CLASS(agg);
    GstPadTemplate *pad_template;
    GstAggregatorPad *new_pad;
    guint stream_index;

    // Ensure direction is correct
    if (templ->direction != GST_PAD_SINK || sscanf (name, meta_sink_factory.name_template, &stream_index) < 1) {
        GST_ERROR_OBJECT(agg, "Only '%s' pads can be created dynamically", meta_sink_factory.name_template);
        return NULL;
    }

    // Get the pad template by name
    pad_template = gst_element_class_get_pad_template(klass, templ->name_template);
    if (!pad_template) {
        GST_ERROR_OBJECT(agg, "No template found for pad name '%s'", name);
        return NULL;
    }

    // Create the dynamic pad from the template
    // Create a new GstAggregatorPad
    new_pad = GST_AGGREGATOR_PAD(g_object_new(GST_TYPE_AGGREGATOR_PAD,
                           "name", name,
                           "direction", templ->direction,
                           NULL));

    // Add the new pad to the list of dynamic sink pads
    GstAnalyticsAggregator *self = GST_ANALYTICS_AGGREGATOR(agg);
    self->dynamic_sink_pads = g_list_append(self->dynamic_sink_pads, new_pad);
    
    GST_DEBUG_OBJECT(agg, "Created new pad: %s", name);
    return new_pad;
}

static void my_element_release_pad(GstElement *element, GstPad *pad) {
    GstAnalyticsAggregator *self = GST_ANALYTICS_AGGREGATOR(element);

    GST_INFO_OBJECT(self, "Releasing pad: %s", GST_PAD_NAME(pad));
    
    auto pad_name = gst_pad_get_name(pad);
    if (g_strcmp0(pad_name, video_sink_factory.name_template) == 0 && self->dynamic_sink_pads) {
        GST_WARNING_OBJECT(pad,
            "%s pad is being released, while other sink pads are still linked. "
            "The pipeline may not progress as long as %s is unlinked.",
            pad_name, pad_name);
    }

    self->dynamic_sink_pads = g_list_remove(self->dynamic_sink_pads, pad);
    
    gst_element_remove_pad(element, pad);
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