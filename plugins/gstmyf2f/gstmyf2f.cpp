#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>

#include <atomic>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>

#define GST_CAT_DEFAULT gst_myf2f_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

typedef struct _GstMyF2F {
  GstBaseTransform parent;

  gchar* config_path;
  std::unordered_map<std::string, std::string> config_kv;
  std::mutex config_mutex;

  std::atomic<bool> printed_once;
} GstMyF2F;

typedef struct _GstMyF2FClass {
  GstBaseTransformClass parent_class;
} GstMyF2FClass;

G_DEFINE_TYPE (GstMyF2F, gst_myf2f, GST_TYPE_BASE_TRANSFORM);

// ---------- Properties ----------
enum {
  PROP_0 = 0,
  PROP_CONFIG_PATH,
};

static void gst_myf2f_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec) {
  auto* self = (GstMyF2F*)object;
  switch (prop_id) {
    case PROP_CONFIG_PATH: {
      const gchar* p = g_value_get_string(value);
      std::lock_guard<std::mutex> lock(self->config_mutex);
      g_free(self->config_path);
      self->config_path = p ? g_strdup(p) : nullptr;
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void gst_myf2f_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec) {
  auto* self = (GstMyF2F*)object;
  switch (prop_id) {
    case PROP_CONFIG_PATH:
      g_value_set_string(value, self->config_path);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void gst_myf2f_finalize(GObject* object) {
  auto* self = (GstMyF2F*)object;
  std::lock_guard<std::mutex> lock(self->config_mutex);
  g_free(self->config_path);
  self->config_path = nullptr;
  G_OBJECT_CLASS(gst_myf2f_parent_class)->finalize(object);
}

// ---------- Config parsing ----------
static void parse_config_file(GstMyF2F* self) {
  std::unordered_map<std::string, std::string> kv;
  gchar* path_copy = nullptr;
  {
    std::lock_guard<std::mutex> lock(self->config_mutex);
    if (!self->config_path) {
      GST_INFO_OBJECT(self, "No config-path set; using defaults.");
      return;
    }
    path_copy = g_strdup(self->config_path);
  }

  std::ifstream f(path_copy);
  if (!f.good()) {
    GST_WARNING_OBJECT(self, "Cannot open config file: %s", path_copy);
    g_free(path_copy);
    return;
  }

  std::string line;
  while (std::getline(f, line)) {
    auto trim = [](std::string& s){
      const char* ws = " \t\r\n";
      auto b = s.find_first_not_of(ws);
      auto e = s.find_last_not_of(ws);
      if (b == std::string::npos) { s.clear(); return; }
      s = s.substr(b, e - b + 1);
    };
    trim(line);
    if (line.empty() || line[0] == '#') continue;
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string k = line.substr(0, eq);
    std::string v = line.substr(eq + 1);
    trim(k); trim(v);
    if (!k.empty()) kv[k] = v;
  }

  {
    std::lock_guard<std::mutex> lock(self->config_mutex);
    self->config_kv = std::move(kv);
  }
  GST_INFO_OBJECT(self, "Parsed config file: %s", path_copy);
  g_free(path_copy);
}

// ---------- Start/Stop ----------
static gboolean gst_myf2f_start(GstBaseTransform* base) {
  auto* self = (GstMyF2F*)base;
  self->printed_once.store(false, std::memory_order_relaxed);
  parse_config_file(self);
  return TRUE;
}

static gboolean gst_myf2f_stop(GstBaseTransform* /*base*/) {
  return TRUE;
}

// ---------- Caps ----------
static gboolean gst_myf2f_set_caps(GstBaseTransform* /*base*/, GstCaps* incaps, GstCaps* outcaps) {
  GST_DEBUG("Negotiated caps: in=%" GST_PTR_FORMAT " out=%" GST_PTR_FORMAT, incaps, outcaps);
  return TRUE;
}

// ---------- Transform (in-place) ----------
static GstFlowReturn gst_myf2f_transform_ip(GstBaseTransform* base, GstBuffer* buf) {
  auto* self = (GstMyF2F*)base;

  if (!self->printed_once.exchange(true, std::memory_order_acq_rel)) {
    g_print("[gstmyf2f] transform_ip: success\n");
  }

  return GST_FLOW_OK;
}

// ---------- Class/Init ----------
static void gst_myf2f_class_init(GstMyF2FClass* klass) {
  GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
  GstElementClass* gstelement_class = GST_ELEMENT_CLASS(klass);
  GstBaseTransformClass* trans_class = GST_BASE_TRANSFORM_CLASS(klass);

  gobject_class->set_property = gst_myf2f_set_property;
  gobject_class->get_property = gst_myf2f_get_property;
  gobject_class->finalize = gst_myf2f_finalize;

  g_object_class_install_property(
      gobject_class, PROP_CONFIG_PATH,
      g_param_spec_string("config-path",
                          "Config file path",
                          "Path to key=value config file",
                          nullptr,
                          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_set_static_metadata(
      gstelement_class,
      "My Frame2Frame (in-place) filter",
      "Filter/Video",
      "Minimal in-place frame processor with config parsing",
      "Your Name <you@example.com>");

  GstCaps* caps = gst_caps_from_string(
    "video/x-raw(memory:NVMM), format=(string){ NV12, I420, RGB }, "
    "width=(int)[ 16, 8192 ], height=(int)[ 16, 8192 ], "
    "framerate=(fraction)[ 0/1, 240/1 ]; "
    "video/x-raw, format=(string){ NV12, I420, RGB }, "
    "width=(int)[ 16, 8192 ], height=(int)[ 16, 8192 ], "
    "framerate=(fraction)[ 0/1, 240/1 ]");

  gst_element_class_add_pad_template(
      gstelement_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_ref(caps)));
  gst_element_class_add_pad_template(
      gstelement_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps));

  trans_class->start        = gst_myf2f_start;
  trans_class->stop         = gst_myf2f_stop;
  trans_class->set_caps     = gst_myf2f_set_caps;
  trans_class->transform_ip = gst_myf2f_transform_ip;

//   gst_base_transform_class_set_in_place(trans_class, TRUE);
//   gst_base_transform_class_set_passthrough_on_same_caps(trans_class, TRUE);
}

static void gst_myf2f_init(GstMyF2F* self) {
  self->config_path = nullptr;
  self->printed_once.store(false, std::memory_order_relaxed);
}

// ---------- Plugin entry ----------
static gboolean plugin_init(GstPlugin* plugin) {
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "myf2f", 0, "myf2f");
  return gst_element_register(plugin, "myf2f", GST_RANK_NONE, gst_myf2f_get_type());
}

#define PACKAGE "myf2f"
GST_PLUGIN_DEFINE(
  GST_VERSION_MAJOR,
  GST_VERSION_MINOR,
  myf2f,
  "My Frame2Frame in-place filter plugin",
  plugin_init,
  "1.0.0",
  "LGPL",
  "gstmyf2f",
  "https://example.com"
)
