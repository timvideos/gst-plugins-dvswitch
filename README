# gst-dvswitch #

`gst-dvswitch` is a GStreamer plugin for acquiring a DIF (DV) stream from a
dvswitch server.

The plugin does not require dvswitch to be installed on the same machine.

This plugin borrows code quite heavily from udpsrc.

# Building #

This contains the usual autotools-based build.  Use `./autogen.sh` to create
`./configure` and related scripts.

# How to use #

You can use it from the commandline like so:

`gst-launch-0.10 -v dvswitchsrc hostname=127.0.0.1 port=5000 ! dvdemux ! dvdec ! autovideosink`

If a hostname and port are not specified, it will default to `127.0.0.1:5000`.

The source also contains a URI handler, so you can open it with something like
`totem` by selecting `File` -> `Open Location...` and entering a URI:

`dvswitch://127.0.0.1:5000`

You could also use this with flumotion.

# `dvswitch` protocol #

dvswitch's protocol is documented in `protocol.h` (in the dvswitch sources).  How
it works is by sending one of the following four character "greetings" to the
server, at which point you can start sending / reciving DV frames.

# Hacking #

Development is done out of my github repository: https://github.com/micolous/gst-dvswitch/

## TODO ##

* Fix Win32 support.
* Handle disconnects better.
* Documentation.
* Clean out remaining UDP-specific code.

