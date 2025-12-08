#!/bin/bash
# export GST_DEBUG=4
gst-launch-1.0 -vvv videotestsrc ! videoconvert ! myf2f config-path=/home/fronti/gst-plugins/plugins/gstmyf2f/resources/config-example.yaml ! videoconvert ! fakesink