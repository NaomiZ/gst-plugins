#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/base/gstaggregator.h>
#include <nvdsmeta.h>
#include <nvbufsurface.h>
#include <gstnvdsmeta.h>
#include "gstanalyticsaggregator.h"
#include <stdio.h>
#include <utility>

#define PLUGIN_NAME "analyticsaggregator"

G_DEFINE_TYPE(GstAnalyticsAggregator, gst_analytics_aggregator, GST_TYPE_AGGREGATOR);

static GstFlowReturn gst_analytics_aggregator_update_src_caps(GstAggregator *aggregator, GstCaps *caps, GstCaps **ret);
static gboolean gst_analytics_aggregator_sink_query(GstAggregator *parent, GstAggregatorPad *pad, GstQuery *query);
static GstAggregatorPad *gst_analytics_aggregator_create_new_pad(GstAggregator *agg, GstPadTemplate *templ, const gchar *name, const GstCaps *caps);
static gboolean gst_analytics_aggregator_sink_event(GstAggregator *agg, GstAggregatorPad *pad, GstEvent *event);
static GstFlowReturn gst_analytics_aggregator_aggregate(GstAggregator *agg, gboolean timeout);
static void gst_analytics_release_pad(GstElement *element, GstPad *pad);
static void gst_analytics_aggregator_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void gst_analytics_aggregator_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);

#define GST_CAPS_FEATURE_MEMORY_NVMM "memory:NVMM"

static GstStaticPadTemplate video_sink_factory =
    GST_STATIC_PAD_TEMPLATE("video_sink",
                            GST_PAD_SINK,
                            GST_PAD_ALWAYS,
                            GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE_WITH_FEATURES(GST_CAPS_FEATURE_MEMORY_NVMM,
                                                                              "{ NV12, RGBA, I420 }")));

static GstStaticPadTemplate meta_sink_factory =
    GST_STATIC_PAD_TEMPLATE("sink_%u",
                            GST_PAD_SINK,
                            GST_PAD_REQUEST,
                            GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE_WITH_FEATURES(GST_CAPS_FEATURE_MEMORY_NVMM,
                                                                              "{ NV12, RGBA, I420 }")));

static GstStaticPadTemplate src_factory =
    GST_STATIC_PAD_TEMPLATE("src",
                            GST_PAD_SRC,
                            GST_PAD_ALWAYS,
                            GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE_WITH_FEATURES(GST_CAPS_FEATURE_MEMORY_NVMM,
                                                                              "{ NV12, RGBA, I420 }")));

enum
{
    PROP_0,
    PROP_SILENT,
    PROP_LAST
};

static void gst_analytics_aggregator_class_init(GstAnalyticsAggregatorClass *klass)
{

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
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
    element_class->release_pad = GST_DEBUG_FUNCPTR(gst_analytics_release_pad);

    gobject_class->set_property = gst_analytics_aggregator_set_property;
    gobject_class->get_property = gst_analytics_aggregator_get_property;

    g_object_class_install_property(gobject_class, PROP_SILENT,
                                    g_param_spec_boolean("silent", "Silent", "Silent prints",
                                                         FALSE, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    // Add pad templates for the video sink, dynamic sink, and source pads
    gst_element_class_add_static_pad_template_with_gtype(element_class, &video_sink_factory, GST_TYPE_AGGREGATOR_PAD);
    gst_element_class_add_static_pad_template_with_gtype(element_class, &meta_sink_factory, GST_TYPE_AGGREGATOR_PAD);
    gst_element_class_add_static_pad_template_with_gtype(element_class, &src_factory, GST_TYPE_AGGREGATOR_PAD);
}

static void gst_analytics_aggregator_init(GstAnalyticsAggregator *self)
{
    self->video_sink_pad = GST_AGGREGATOR_PAD(g_object_new(GST_TYPE_AGGREGATOR_PAD,
                                                           "name", "video_sink",
                                                           "direction", GST_PAD_SINK, NULL));
    gst_element_add_pad(GST_ELEMENT(self), GST_PAD(self->video_sink_pad));

    self->dynamic_sink_pads = NULL;
}

static void gst_analytics_aggregator_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    GstAnalyticsAggregator *self = GST_ANALYTICS_AGGREGATOR(object);

    switch (prop_id)
    {
    case PROP_SILENT:
        g_value_set_boolean(value, self->silent);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gst_analytics_aggregator_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    GstAnalyticsAggregator *self = GST_ANALYTICS_AGGREGATOR(object);
    switch (prop_id)
    {
    case PROP_SILENT:
        self->silent = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static gboolean gst_analytics_aggregator_sink_event(GstAggregator *agg, GstAggregatorPad *pad, GstEvent *event)
{
    // Implement event handling logic here
    switch (GST_EVENT_TYPE(event))
    {
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

static std::pair<uint32_t, uint32_t> get_frame_dimensions_from_buffer(GstBuffer *buffer)
{
    GstMapInfo map = {};
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ))
    {
        GST_ERROR("Failed to map buffer");
        return {0, 0};
    }

    NvBufSurface *surface = (NvBufSurface *)map.data;
    if (!surface)
    {
        GST_ERROR("Failed to get NvBufSurface from buffer");
        gst_buffer_unmap(buffer, &map);
        return {0, 0};
    }

    uint32_t frame_width = surface->surfaceList->width;
    uint32_t frame_height = surface->surfaceList->height;

    gst_buffer_unmap(buffer, &map);
    return {frame_width, frame_height};
}
static GstFlowReturn gst_analytics_aggregator_aggregate(GstAggregator *agg, gboolean timeout)
{
    GstBuffer *outbuf = NULL;
    GstBuffer *inbuf = NULL;
    GstAggregatorPad *pad;
    GstClockTime pts = GST_CLOCK_TIME_NONE;
    GstClockTime dts = GST_CLOCK_TIME_NONE;
    GstClockTime duration = GST_CLOCK_TIME_NONE;
    uint8_t pads_counter = 1;

    GstAnalyticsAggregator *self = GST_ANALYTICS_AGGREGATOR(agg);
    outbuf = gst_aggregator_pad_pop_buffer(self->video_sink_pad);
    if (!outbuf)
    {
        GST_ERROR_OBJECT(self, "Failed to pop buffer from video sink pad");
        return GST_FLOW_ERROR;
    }

    pts = GST_BUFFER_PTS(outbuf);
    dts = GST_BUFFER_DTS(outbuf);
    duration = GST_BUFFER_DURATION(outbuf);

    std::pair<uint32_t,uint32_t> frame_dim = get_frame_dimensions_from_buffer(outbuf);
    if (frame_dim.first == 0 || frame_dim.second == 0)
    {
        GST_ERROR_OBJECT(self, "Failed to get frame dimensions from buffer");
        return GST_FLOW_ERROR;
    }

    for (GList *walk = self->dynamic_sink_pads; walk; walk = g_list_next(walk))
    {
        pad = GST_AGGREGATOR_PAD(walk->data);
        if (pad)
        {
            pads_counter++;
            inbuf = gst_aggregator_pad_pop_buffer(pad);
            if (!inbuf)
            {
                GST_WARNING_OBJECT(self, "Failed to pop buffer from pad: %s", GST_PAD_NAME(pad));
            }
            else
            {
                if (!self->silent)
                {
                    g_print("analyticsaggregator::aggregate - pad: %s\n", GST_PAD_NAME(pad));
                }
                
                std::pair<uint32_t,uint32_t> in_frame_dim = get_frame_dimensions_from_buffer(outbuf);
                if (in_frame_dim.first == 0 || in_frame_dim.second == 0)
                {
                    GST_ERROR_OBJECT(self, "Failed to get frame dimensions from buffer");
                    return GST_FLOW_ERROR;
                }

                std::pair<uint32_t,uint32_t> scale_factor = {frame_dim.first / in_frame_dim.first, frame_dim.second / in_frame_dim.second};
                if (!self->silent)
                {
                    g_print("analyticsaggregator::aggregate - scale factor: %d, %d\n", scale_factor.first, scale_factor.second);
                }

                // Scale the inbuf object metadata to the outbuf object metadata
                NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta(inbuf);
                if (batch_meta)
                {
                    for (NvDsMetaList *l_frame = batch_meta->frame_meta_list; l_frame != NULL; l_frame = l_frame->next)
                    {
                        NvDsFrameMeta *frame_meta = (NvDsFrameMeta *)l_frame->data;
                        if (frame_meta)
                        {
                            for (NvDsMetaList *l_obj = frame_meta->obj_meta_list; l_obj != NULL; l_obj = l_obj->next)
                            {
                                NvDsObjectMeta *obj_meta = (NvDsObjectMeta *)l_obj->data;
                                if (obj_meta)
                                {
                                    obj_meta->rect_params.left *= scale_factor.first;
                                    obj_meta->rect_params.top *= scale_factor.second;
                                    obj_meta->rect_params.width *= scale_factor.first;
                                    obj_meta->rect_params.height *= scale_factor.second;
                                }
                            }
                        }
                    }
                }
                else
                {
                    GST_WARNING_OBJECT(self, "No batch meta from buffer from pad: %s", GST_PAD_NAME(pad));
                }

            }
        }
        else
        {
            GST_WARNING_OBJECT(self, "Found null pad in dynamic sink pad list, removing pad from list");
            self->dynamic_sink_pads = g_list_remove(self->dynamic_sink_pads, pad);
        }
    }
    if (!self->silent)
    {
        g_print("analyticsaggregator::aggregate - %d pads\n", pads_counter);
    }

    gst_aggregator_selected_samples(agg, pts, dts, duration, NULL);
    //gst_aggregator_finish_buffer(GST_AGGREGATOR(self), outbuf);
    //pass a fake buffer to the next element
    gst_aggregator_finish_buffer(GST_AGGREGATOR(self), outbuf);
    return GST_FLOW_OK;
}

GstFlowReturn gst_analytics_aggregator_update_src_caps(GstAggregator *aggregator, GstCaps *caps, GstCaps **ret)
{
    GstAnalyticsAggregator *self = GST_ANALYTICS_AGGREGATOR(aggregator);
    GstCaps *video_sink_caps = gst_pad_get_current_caps(GST_PAD(self->video_sink_pad));
    if (!video_sink_caps || gst_caps_is_empty(video_sink_caps))
    {
        GST_WARNING_OBJECT(aggregator, "Video sink pad caps are not set, using default caps");
        video_sink_caps = gst_caps_new_simple("video/x-raw",
                                              "format", G_TYPE_STRING, "NV12",
                                              "width", G_TYPE_INT, 320,
                                              "height", G_TYPE_INT, 240,
                                              "framerate", GST_TYPE_FRACTION, 30, 1,
                                              NULL);
    }

    if (!gst_caps_can_intersect(video_sink_caps, caps))
    {
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

    switch (GST_QUERY_TYPE(query))
    {
    case GST_QUERY_CAPS:
    {
        GstCaps *filter;
        gst_query_parse_caps(query, &filter);
        GstCaps *caps = gst_pad_get_pad_template_caps(GST_PAD(pad));
        if (filter)
        {
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
    if (templ->direction != GST_PAD_SINK || sscanf(name, meta_sink_factory.name_template, &stream_index) < 1)
    {
        GST_ERROR_OBJECT(agg, "Only '%s' pads can be created dynamically", meta_sink_factory.name_template);
        return NULL;
    }

    // Get the pad template by name
    pad_template = gst_element_class_get_pad_template(klass, templ->name_template);
    if (!pad_template)
    {
        GST_ERROR_OBJECT(agg, "No template found for pad name '%s'", name);
        return NULL;
    }

    new_pad = GST_AGGREGATOR_PAD(gst_pad_new_from_template(pad_template, name));

    // Add the new pad to the list of dynamic sink pads
    GstAnalyticsAggregator *self = GST_ANALYTICS_AGGREGATOR(agg);
    self->dynamic_sink_pads = g_list_append(self->dynamic_sink_pads, new_pad);

    GST_DEBUG_OBJECT(agg, "Created new pad: %s", name);
    return new_pad;
}

static void gst_analytics_release_pad(GstElement *element, GstPad *pad)
{
    GstAnalyticsAggregator *self = GST_ANALYTICS_AGGREGATOR(element);

    GST_INFO_OBJECT(self, "Releasing pad: %s", GST_PAD_NAME(pad));

    auto pad_name = gst_pad_get_name(pad);
    if (g_strcmp0(pad_name, video_sink_factory.name_template) == 0 && self->dynamic_sink_pads)
    {
        GST_WARNING_OBJECT(pad,
                           "%s pad is being released, while other sink pads are still linked. "
                           "The pipeline may not progress as long as %s is unlinked.",
                           pad_name, pad_name);
    }

    self->dynamic_sink_pads = g_list_remove(self->dynamic_sink_pads, pad);

    gst_element_remove_pad(element, pad);
    if (!self->silent)
    {
        g_print("%s pad is released from analytics aggregator", pad_name);
    }
}

static gboolean plugin_init(GstPlugin *plugin)
{
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