# analyticsaggregator

The analyticsaggregator is a custom GStreamer plugin designed to merge metadata from secondary streams into a primary video stream. It uses NVIDIA's DeepStream NvDsBatchMeta for metadata handling and ensures proper synchronization and frame alignment.
Features


## Build Instructions
### Dependencies

Ensure the following are installed:

    GStreamer (1.0+)
    NVIDIA DeepStream SDK (for nvdsmeta)
    CMake (3.10+)

### Steps

#### Clone the repository:

git clone <repository-url>
cd analyticsaggregator

#### Create a build directory and run CMake:

mkdir build && cd build
cmake ..

#### Build the plugin:

make

#### Install the plugin:

    sudo make install

### Usage

    Ensure the plugin is in your GStreamer plugin path (e.g., /usr/lib/gstreamer-1.0/).
    Test the plugin in a pipeline:

    gst-launch-1.0 videotestsrc ! "video/x-raw(memory:NVMM),format=NV12,width=1280,height=720" ! \
    analyticsaggregator name=agg \
    agg.video_sink::video_sink \
    agg.sink_0::metadata_sink \
    agg.src ! fakesink

    gst-launch-1.0 videotestsrc ! nvvidconv ! analyticsaggregator ! fakesink
    gst-launch-1.0 videotestsrc ! nvvidconv ! tee name=t t. ! agg.video_sink analyticsaggregator name=agg ! fakesink t. ! agg.sink_0
### Error Handling

#### Mismatched Capabilities:
        If input streams have mismatched formats, the plugin logs an error and rejects the CAPS negotiation.
#### Missing Metadata:
        If no metadata is available for a frame, the plugin passes the video frame downstream without modification.
#### Metadata Merging Failure:
        If metadata merging fails, the video buffer is passed downstream with a warning logged.