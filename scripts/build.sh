#!/bin/bash

cd /home/fronti/gst-plugins/plugins/gstmyf2f
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

cd /home/fronti/gst-plugins/processing-libs/video-stabilization/demo_stabilizer
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
