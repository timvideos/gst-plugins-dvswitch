/* GStreamer
 * (c) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * (c) 2013 Jan Schmidt <jan@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:element-dvswitchsink
 *
 * dvswitchsink is a sink that wraps tcpclientsink to send video to a dvswitch
 * server.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-0.10 -v -m videotestsrc ! avenc_dv ! avmux_dv ! dvswitchsink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstdvswitchsink.h"

GST_DEBUG_CATEGORY_EXTERN (gst_dvswitch_debug);
#define GST_CAT_DEFAULT gst_dvswitch_debug

#define DEFAULT_TS_OFFSET           0
#define DEFAULT_SYNC                TRUE

#define UDP_DEFAULT_HOST        "localhost"
#define UDP_DEFAULT_PORT        4951

/* Properties */
enum
{
  PROP_0,
  PROP_CAPS,
  PROP_TS_OFFSET,
  PROP_SYNC,
  PROP_HOST,
  PROP_PORT
};

static GstStateChangeReturn
gst_dvswitch_sink_change_state (GstElement * element,
    GstStateChange transition);
static void gst_dvswitch_sink_dispose (GstDVSwitchSink * sink);

static void gst_dvswitch_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_dvswitch_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

G_DEFINE_TYPE (GstDVSwitchSink, gst_dvswitch_sink, GST_TYPE_BIN);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS("video/x-dv,systemstream=(boolean)true"));

static void
gst_dvswitch_sink_class_init (GstDVSwitchSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *eklass = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template(eklass, gst_static_pad_template_get(&sink_template));
  gst_element_class_set_details_simple (eklass,
      "DVSwitch video sink",
      "Sink/Video",
      "Sink which uses tcpclientsink to stream to a dvswitch server",
      "Jan Schmidt <jan@centricular.com>");

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = (GObjectFinalizeFunc) gst_dvswitch_sink_dispose;
  gobject_class->set_property = gst_dvswitch_sink_set_property;
  gobject_class->get_property = gst_dvswitch_sink_get_property;

  eklass->change_state = GST_DEBUG_FUNCPTR (gst_dvswitch_sink_change_state);

  g_object_class_install_property (gobject_class, PROP_TS_OFFSET,
      g_param_spec_int64 ("ts-offset", "TS Offset",
          "Timestamp offset in nanoseconds", G_MININT64, G_MAXINT64,
          DEFAULT_TS_OFFSET, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SYNC,
      g_param_spec_boolean ("sync", "Sync",
          "Sync on the clock", DEFAULT_SYNC,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_HOST,
      g_param_spec_string ("host", "host",
          "The host/IP/Multicast group to send the packets to",
          UDP_DEFAULT_HOST, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_PORT,
      g_param_spec_int ("port", "port", "The port to send the packets to",
          0, 65535, UDP_DEFAULT_PORT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

}

static void
gst_dvswitch_sink_dispose (GstDVSwitchSink * sink)
{
  if (sink->probe_id) {
    gst_pad_remove_probe(sink->pad, sink->probe_id);
    sink->probe_id = 0;
  }

  G_OBJECT_CLASS (gst_dvswitch_sink_parent_class)->dispose ((GObject *) sink);
}

static gboolean gst_dvswitch_sink_create_sink(GstDVSwitchSink * sink);
static gboolean
gst_dvswitch_sink_probe(GstPad *pad, GstBuffer *buf, GstDVSwitchSink *sink);

static void
gst_dvswitch_sink_init (GstDVSwitchSink * sink)
{
  GstPadTemplate *t = gst_static_pad_template_get (&sink_template);

  sink->pad = gst_ghost_pad_new_no_target_from_template ("sink", t);
  gst_element_add_pad (GST_ELEMENT (sink), sink->pad);
  gst_object_unref (t);

  sink->ts_offset = DEFAULT_TS_OFFSET;
  sink->sync = DEFAULT_SYNC;

  /* mark as sink */
  GST_OBJECT_FLAG_SET (sink, GST_ELEMENT_FLAG_SINK);

  gst_dvswitch_sink_create_sink(sink);

  sink->probe_id = gst_pad_add_probe(sink->pad, GST_PAD_PROBE_TYPE_BUFFER, (GstPadProbeCallback)gst_dvswitch_sink_probe, sink, NULL);
}

// sporked from dvswitch/src/protocol.h
// Client initially sends a greeting of this length identifying which
// protocol it's going to speak.
#define GREETING_SIZE 4
// Source which sends a raw DIF stream.
#define GREETING_RAW_SOURCE "SORC"

static gboolean
gst_dvswitch_sink_probe(GstPad *pad, GstBuffer *buf, GstDVSwitchSink *sink)
{
  if (sink->need_greeting) {
    GstBuffer *buf;
    GstPad *targetpad;

    GST_DEBUG_OBJECT (sink, "Sending DVSwitch greeting packet");
    buf = gst_buffer_new_and_alloc(GREETING_SIZE);
    gst_buffer_fill (buf, 0, GREETING_RAW_SOURCE, GREETING_SIZE);

    targetpad = gst_element_get_static_pad (sink->kid, "sink");
    gst_pad_chain(targetpad, buf);
    gst_object_unref(targetpad);

    sink->need_greeting = FALSE;
  }
  return TRUE;
}

static gboolean
gst_dvswitch_sink_create_sink(GstDVSwitchSink * sink)
{
  GstElement *esink;
  GstPad *targetpad;

  /* find element */
  GST_DEBUG_OBJECT (sink, "Creating new kid");
  if (!(esink = gst_element_factory_make("tcpclientsink", "sink")))
    goto no_sink;

  g_object_set (G_OBJECT (esink), "ts-offset", sink->ts_offset, NULL);
  g_object_set (G_OBJECT (esink), "sync", sink->sync, NULL);

  sink->kid = esink;
  gst_bin_add (GST_BIN (sink), esink);

  /* attach ghost pad */
  GST_DEBUG_OBJECT (sink, "Re-assigning ghostpad");
  targetpad = gst_element_get_static_pad (sink->kid, "sink");
  if (!gst_ghost_pad_set_target (GST_GHOST_PAD (sink->pad), targetpad))
    goto target_failed;

  gst_object_unref (targetpad);
  GST_DEBUG_OBJECT (sink, "done changing auto video sink");

  return TRUE;

  /* ERRORS */
no_sink:
  {
    GST_ELEMENT_ERROR (sink, LIBRARY, INIT, (NULL),
        ("Failed to create tcpclientsink"));
    return FALSE;
  }
target_failed:
  {
    GST_ELEMENT_ERROR (sink, LIBRARY, INIT, (NULL),
        ("Failed to set target pad"));
    gst_object_unref (targetpad);
    return FALSE;
  }
}

static GstStateChangeReturn
gst_dvswitch_sink_change_state (GstElement * element,
    GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstDVSwitchSink *sink = GST_DVSWITCH_SINK (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      sink->need_greeting = TRUE;
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (gst_dvswitch_sink_parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_dvswitch_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDVSwitchSink *sink = GST_DVSWITCH_SINK (object);

  switch (prop_id) {
    case PROP_TS_OFFSET:
      sink->ts_offset = g_value_get_int64 (value);
      if (sink->kid)
        g_object_set_property (G_OBJECT (sink->kid), pspec->name, value);
      break;
    case PROP_SYNC:
      sink->sync = g_value_get_boolean (value);
      if (sink->kid)
        g_object_set_property (G_OBJECT (sink->kid), pspec->name, value);
      break;

    case PROP_HOST:
    case PROP_PORT:
      g_object_set_property (G_OBJECT (sink->kid), pspec->name, value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_dvswitch_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDVSwitchSink *sink = GST_DVSWITCH_SINK (object);

  switch (prop_id) {
    case PROP_TS_OFFSET:
      g_value_set_int64 (value, sink->ts_offset);
      break;
    case PROP_SYNC:
      g_value_set_boolean (value, sink->sync);
      break;
    case PROP_HOST:
    case PROP_PORT:
      g_object_get_property (G_OBJECT (sink->kid), pspec->name, value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
