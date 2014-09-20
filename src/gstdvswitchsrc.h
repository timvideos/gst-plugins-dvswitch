/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2012 Michael Farrell <michael@uanywhere.com.au>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef __GST_DVSWITCHSRC_H__
#define __GST_DVSWITCHSRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>

G_BEGIN_DECLS

#include <errno.h>
#include <string.h>
// sporked from gstudpnetutils.h
#include <sys/types.h>

/* Needed for G_OS_XXXX */
#include <glib.h>

#ifdef G_OS_WIN32
/* ws2_32.dll has getaddrinfo and freeaddrinfo on Windows XP and later.
 * minwg32 headers check WINVER before allowing the use of these */
#ifndef WINVER
#define WINVER 0x0501
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#ifndef socklen_t
#define socklen_t int
#endif

/* Needed for GstObject and GST_WARNING_OBJECT */
#include <gst/gstobject.h>
#include <gst/gstinfo.h>

#else
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/ioctl.h>
#endif

#include <fcntl.h>

#ifdef G_OS_WIN32
#warning Win32 support is currently broken.

#define IOCTL_SOCKET ioctlsocket
#define CLOSE_SOCKET(sock) closesocket(sock)
#define setsockopt(sock,l,opt,val,len) setsockopt(sock,l,opt,(char *)(val),len)
//#define WSA_STARTUP(obj) gst_udp_net_utils_win32_wsa_startup(GST_OBJECT(obj))
#define WSA_CLEANUP(obj) WSACleanup ()

#else

#define IOCTL_SOCKET ioctl
#define CLOSE_SOCKET(sock) close(sock)
#define setsockopt(sock,l,opt,val,len) setsockopt(sock,l,opt,(void *)(val),len)
#define WSA_STARTUP(obj)
#define WSA_CLEANUP(obj)

#endif

/*
#ifdef G_OS_WIN32
gboolean gst_udp_net_utils_win32_wsa_startup (GstObject * obj);
#endif
*/


// sporked from dvswitch/src/protocol.h
// Client initially sends a greeting of this length identifying which
// protocol it's going to speak.
#define GREETING_SIZE 4
// Sink which receives a raw DIF stream.
#define GREETING_RAW_SINK "RSNK"

/* #defines don't like whitespacey bits */
#define GST_TYPE_DVSWITCHSRC \
  (gst_dvswitch_src_get_type())
#define GST_DVSWITCHSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DVSWITCHSRC,GstDvswitchSrc))
#define GST_DVSWITCHSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DVSWITCHSRC,GstDvswitchSrcClass))
#define GST_IS_DVSWITCHSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DVSWITCHSRC))
#define GST_IS_DVSWITCHSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DVSWITCHSRC))

typedef struct {
  gchar    *host;
  gint      port;
  gboolean  is_ipv6;
} GstDvswitchUri;

typedef struct _GstDvswitchSrc      GstDvswitchSrc;
typedef struct _GstDvswitchSrcClass GstDvswitchSrcClass;

struct _GstDvswitchSrc
{
  GstPushSrc parent;

  GstPad *srcpad;

  GstDvswitchUri  uri;
  gchar *uristr;

//  gchar *hostname;
//  guint16 port;
  guint64 timeout;
  gint buffer_size;

  GstPollFD sock;
  GstPoll *fdset;
  int sockfd;
  struct   sockaddr_storage myaddr;

};

struct _GstDvswitchSrcClass
{
  GstPushSrcClass parent_class;
};

GType gst_dvswitch_src_get_type (void);

G_END_DECLS

#endif /* __GST_DVSWITCHSRC_H__ */
