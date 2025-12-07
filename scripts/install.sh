#!/bin/bash

sudo rm ${GST_PLUGIN_PATH}/libgstmyf2f.so
cd /home/fronti/gst-plugins/plugins/gstmyf2f/build
sudo cp libgstmyf2f.so ${GST_PLUGIN_PATH}/
gst-inspect-1.0 myf2f