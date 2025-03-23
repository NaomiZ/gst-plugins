#ifndef __GST_ANALYTICS_AGGREGATOR_H__
#define __GST_ANALYTICS_AGGREGATOR_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/base/gstaggregator.h>

G_BEGIN_DECLS

#define GST_TYPE_ANALYTICS_AGGREGATOR (gst_analytics_aggregator_get_type())
G_DECLARE_FINAL_TYPE(GstAnalyticsAggregator, gst_analytics_aggregator, GST, ANALYTICS_AGGREGATOR, GstAggregator);

struct _GstAnalyticsAggregator {
    GstAggregator parent;
    GstAggregatorPad *video_sink_pad;
    GList *dynamic_sink_pads;
    GstAggregatorPad *src_pad;

    gboolean silent;
};

struct _GstAnalyticsAggregatorClass {
    GstAggregatorClass parent_class;
};

GType gst_analytics_aggregator_get_type(void);

G_END_DECLS

#endif /* __GST_ANALYTICS_AGGREGATOR_H__ */