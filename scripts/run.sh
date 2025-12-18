#!/bin/bash
export GST_DEBUG=4

gst-launch-1.0 -vvv v4l2src device=/dev/video0 ! nvvideoconvert copy-hw=2 ! "video/x-raw(memory:NVMM),format=NV12,width=640,height=480,framerate=30/1" ! myf2f config-path=/home/fronti/gst-plugins/plugins/gstmyf2f/resources/config-nv.yaml ! nvvideoconvert copy-hw=2 ! nv3dsink