/* GStreamer
 * (c) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
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

#ifndef __GST_DVSWITCH_SINK_H__
#define __GST_DVSWITCH_SINK_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_DVSWITCH_SINK \
  (gst_dvswitch_sink_get_type ())
#define GST_DVSWITCH_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_DVSWITCH_SINK, \
                               GstDVSwitchSink))
#define GST_DVSWITCH_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_DVSWITCH_SINK, \
                            GstDVSwitchSinkClass))
#define GST_IS_DVSWITCH_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_DVSWITCH_SINK))
#define GST_IS_DVSWITCH_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_DVSWITCH_SINK))

typedef struct _GstDVSwitchSink {
  GstBin parent;

  /* explicit pointers to stuff used */
  GstPad *pad;
  GstElement *kid;
  GstClockTimeDiff ts_offset;
  gboolean sync;

  gulong probe_id;
  gboolean need_greeting;
  gboolean c3voc_mode;
  gint c3voc_source_id;
} GstDVSwitchSink;

typedef struct _GstDVSwitchSinkClass {
  GstBinClass parent_class;
} GstDVSwitchSinkClass;

GType   gst_dvswitch_sink_get_type    (void);

G_END_DECLS

#endif /* __GST_DVSWITCH_SINK_H__ */
