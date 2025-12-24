#!/bin/bash
set -euo pipefail

# Optional: uncomment for more logging
# export GST_DEBUG=3

# Ensure GStreamer can find our built plugin without system install
PLUGIN_BUILD_DIR="/home/fronti/gst-plugins/plugins/gstmyf2f/build"
export GST_PLUGIN_PATH="${GST_PLUGIN_PATH-}"
if [[ -d "$PLUGIN_BUILD_DIR" ]]; then
	if [[ -n "${GST_PLUGIN_PATH}" ]]; then
		export GST_PLUGIN_PATH="${GST_PLUGIN_PATH}:$PLUGIN_BUILD_DIR"
	else
		export GST_PLUGIN_PATH="$PLUGIN_BUILD_DIR"
	fi
fi

# Ensure VPI runtime libs are discoverable at runtime (Jetson default path)
VPI_LIB_DIR="/opt/nvidia/vpi3/lib/aarch64-linux-gnu"
if [[ -d "$VPI_LIB_DIR" ]]; then
	export LD_LIBRARY_PATH="${LD_LIBRARY_PATH-}"
	if [[ -n "${LD_LIBRARY_PATH}" ]]; then
		export LD_LIBRARY_PATH="${VPI_LIB_DIR}:${LD_LIBRARY_PATH}"
	else
		export LD_LIBRARY_PATH="${VPI_LIB_DIR}"
	fi
fi

# Allow optional duration via --duration=SECONDS or --frames=NUM to auto-stop for tests
DURATION=""
NUM_FRAMES=""
for arg in "$@"; do
	case "$arg" in
		--duration=*) DURATION="${arg#*=}"; shift ;;
		--frames=*) NUM_FRAMES="${arg#*=}"; shift ;;
		*) ;;
	esac
done

echo "Using GST_PLUGIN_PATH=${GST_PLUGIN_PATH:-<default>}"
echo "Launching pipeline..."

# Build v4l2src element with optional num-buffers
V4L2_SRC="v4l2src device=/dev/video0 io-mode=dmabuf do-timestamp=true"
if [[ -n "$NUM_FRAMES" ]]; then
	V4L2_SRC="${V4L2_SRC} num-buffers=${NUM_FRAMES}"
fi

PIPE_COMMON=(
	${V4L2_SRC} !
	queue max-size-buffers=8 max-size-time=0 max-size-bytes=0 leaky=downstream !
	nvvideoconvert copy-hw=2 disable-passthrough=true !
	"video/x-raw(memory:NVMM),format=NV12,width=1280,height=1080,framerate=30/1" !
	queue max-size-buffers=8 max-size-time=0 max-size-bytes=0 leaky=downstream !
	myf2f config-path=/home/fronti/gst-plugins/plugins/gstmyf2f/resources/config-nv.yaml !
	queue max-size-buffers=8 max-size-time=0 max-size-bytes=0 leaky=downstream !
	nvvideoconvert copy-hw=2 disable-passthrough=true !
	nv3dsink sync=false max-lateness=-1 qos=false
)

if [[ -n "$DURATION" ]]; then
	timeout --preserve-status "$DURATION" gst-launch-1.0 -vvv "${PIPE_COMMON[@]}"
else
	gst-launch-1.0 -vvv "${PIPE_COMMON[@]}"
fi

# Alternative resolution/example
# gst-launch-1.0 -vvv v4l2src device=/dev/video0 ! nvvideoconvert copy-hw=2 ! "video/x-raw(memory:NVMM),format=NV12,width=640,height=480,framerate=30/1" ! nvvideoconvert copy-hw=2 ! nv3dsink