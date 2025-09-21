#!/bin/bash

cmake --build build/
sudo cp build/libgstvideostabilizer.so ${GST_PLUGIN_PATH}
gst-launch-1.0 v4l2src device=/dev/video0 num-buffers=3 ! video/x-raw,format=YUY2,width=640,height=480 ! nvvideoconvert ! "video/x-raw(memory:NVMM),format=NV12,width=640,height=480,framerate=30/1" ! videostabilizer ! nvvideoconvert ! fakesink