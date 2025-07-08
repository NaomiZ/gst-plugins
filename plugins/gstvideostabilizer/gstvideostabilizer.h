#ifndef GST_VIDEO_STABILIZER_H
#define GST_VIDEO_STABILIZER_H

#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define GST_TYPE_VIDEO_STABILIZER   (gst_video_stabilizer_get_type())
#define GST_VIDEO_STABILIZER(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_VIDEO_STABILIZER, GstVideoStabilizer))
#define GST_VIDEO_STABILIZER_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_VIDEO_STABILIZER, GstVideoStabilizerClass))
#define GST_IS_VIDEO_STABILIZER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_VIDEO_STABILIZER))
#define GST_IS_VIDEO_STABILIZER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_VIDEO_STABILIZER))

typedef struct _GstVideoStabilizer GstVideoStabilizer;
typedef struct _GstVideoStabilizerClass GstVideoStabilizerClass;

struct _GstVideoStabilizer {
  GstBaseTransform base_trans;
  /* instance members */
};

struct _GstVideoStabilizerClass {
  GstBaseTransformClass base_trans_class;
  /* class members */
};

GType gst_video_stabilizer_get_type(void);

G_END_DECLS

#endif /* GST_VIDEO_STABILIZER_H */