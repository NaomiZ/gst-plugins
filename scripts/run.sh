#!/bin/bash
export GST_DEBUG=4
gst-launch-1.0 -vvv v4l2src device=/dev/video0 ! videoconvert ! myf2f config-path=/home/fronti/gst-plugins/plugins/gstmyf2f/resources/config-basic.yaml ! videoconvert ! autovideosink