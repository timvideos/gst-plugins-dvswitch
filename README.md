# gst-plugins-dvswitch

`gst-plugins-dvswitch` is a GStreamer plugin for talking to DVSwitch server.
Both acquiring and sending a DIF (DV) stream.

The plugin does not require DVSwitch to be installed on the same machine.

 * gstreamer - http://gstreamer.freedesktop.org/
 * DVSwitch - http://dvswitch.alioth.debian.org/wiki/

# Building

This contains the usual autotools-based build. 

Use `./autogen.sh` to create `./configure` and related scripts.

### Building Debian / Ubuntu Package

```
git clone git://github.com/timvideos/gst-plugins-dvswitch.git
cd gst-plugins-dvswitch
dpkg-checkbuilddeps
# Install any missing build dependencies
dpkg-buildpackage -b
dpkg --install ../gstreamer0.10-dvswitch*.deb
```

# How to use

To get video from DVswitch, you can use it from the command-line like so:

```
gst-launch-0.10 -v \
  dvswitchsrc hostname=127.0.0.1 port=5000 ! \
  dvdemux ! \
  dvdec ! \
  autovideosink
```

To send video to a DVswitch, you can use it from the command-line like so;

```
gst-launch-0.10 -v \
  videotestsrc is-live=true ! \
  ffmpegcolorspace ! \
  ffenc_dvvideo ! \
  ffmux_dv ! \
  dvswitchsink host=127.0.0.1 port=5000
```

The source also contains a URI handler, so you can open it with something like
`totem` by selecting `File` -> `Open Location...` and entering a URI:

`dvswitch://127.0.0.1:5000`

### Converting video to DV

Actually converting video to DV is slightly tricky and DVSwitch requires all
inputs to have the *exact* same input format, while gstreamer is happy to send
anything.

See https://github.com/timvideos/dvsource-v4l2-other for the full pipelines.

# DVSwitch protocol

DVSwitch's protocol is documented in `protocol.h` (in the DVSwitch sources).  How
it works is by sending one of the following four character "greetings" to the
server, at which point you can start sending / reciving DV frames.

# Hacking

Development is done out the github repository:
 https://github.com/timvideos/gst-plugins-dvswitch/

Issues can be reported at:
 https://github.com/timvideos/dvsource-v4l2-other/issues
