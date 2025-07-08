# gstvideostabilizer

`gstvideostabilizer` is a GStreamer plugin for real-time video stabilization. It helps reduce unwanted camera shake and produces smoother video output.

## Features

- Real-time video stabilization
- Configurable stabilization strength
- Low-latency processing

## Installation

```sh
meson build
ninja -C build
ninja -C build install
```

## Usage

Example GStreamer pipeline:

```sh
gst-launch-1.0 filesrc location=input.mp4 ! decodebin ! videostabilizer ! videoconvert ! autovideosink
```

## Configuration

You can adjust stabilization parameters using plugin properties. See the plugin documentation for details.

## License

This project is licensed under the MIT License.

## Pipelines

gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw,format=YUY2,width=640,height=480 ! nvvideoconvert ! "video/x-raw(memory:NVMM),format=NV12,width=640,height=480,framerate=30/1" ! videostabilizer ! nvvideoconvert ! nv3dsink