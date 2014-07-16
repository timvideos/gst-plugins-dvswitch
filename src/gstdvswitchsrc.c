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

/**
 * SECTION:element-dvswitchsrc
 *
 * FIXME:Describe dvswitchsrc here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m dvswitchsrc ! dvdemux ! dvdec ! autovideosink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>

#if defined _MSC_VER && (_MSC_VER >= 1400)
#include <io.h>
#endif

#ifdef HAVE_FIONREAD_IN_SYS_FILIO
#include <sys/filio.h>
#endif

#include "gstdvswitchsrc.h"

GST_DEBUG_CATEGORY_EXTERN (gst_dvswitch_debug);
#define GST_CAT_DEFAULT gst_dvswitch_debug

#define DVSWITCH_DEFAULT_HOSTNAME       "127.0.0.1"
#define DVSWITCH_DEFAULT_PORT           5000
#define DVSWITCH_DEFAULT_URI            "dvswitch://"DVSWITCH_DEFAULT_HOSTNAME":"G_STRINGIFY(DVSWITCH_DEFAULT_PORT)
#define DVSWITCH_DEFAULT_SOCK           -1
#define DVSWITCH_DEFAULT_SOCKFD         -1
#define DVSWITCH_DEFAULT_TIMEOUT        0
#define DVSWITCH_DEFAULT_BUFFER_SIZE    0

#define CLOSE_IF_REQUESTED(tcpctx) \
G_STMT_START { \
  CLOSE_SOCKET(tcpctx->sock.fd); \
  tcpctx->sockfd = DVSWITCH_DEFAULT_SOCKFD; \
  tcpctx->sock.fd = DVSWITCH_DEFAULT_SOCK; \
} G_STMT_END
  
  
/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

// properties

enum
{
  PROP_0,
  PROP_HOSTNAME,
  PROP_PORT,
  PROP_URI,
  PROP_BUFFER_SIZE,
  PROP_TIMEOUT,
  
  PROP_LAST
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );
    
static void gst_dvswitch_src_uri_handler_init (gpointer g_iface, gpointer iface_data);

#define gst_dvswitch_src_parent_class parent_class

G_DEFINE_TYPE_WITH_CODE (GstDvswitchSrc, gst_dvswitch_src, GST_TYPE_PUSH_SRC,
      G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER, gst_dvswitch_src_uri_handler_init));


static void gst_dvswitch_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_dvswitch_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
    
static GstFlowReturn gst_dvswitch_src_create (GstPushSrc * psrc, GstBuffer ** buf);
static gboolean gst_dvswitch_src_start (GstBaseSrc * bsrc);
static gboolean gst_dvswitch_src_stop (GstBaseSrc * bsrc);
static gboolean gst_dvswitch_src_unlock (GstBaseSrc * bsrc);
static gboolean gst_dvswitch_src_unlock_stop (GstBaseSrc * bsrc);
static GstCaps *gst_dvswitch_src_get_caps (GstBaseSrc * src, GstCaps * filter);

static void gst_dvswitch_src_finalize (GObject * object);
void gst_dvswitch_uri_init (GstDvswitchUri * uri, const gchar * host, gint port);
int gst_dvswitch_parse_uri (const gchar * uristr, GstDvswitchUri * uri);
static gboolean gst_dvswitch_src_set_uri (GstDvswitchSrc * src, const gchar * uri);
gchar * gst_dvswitch_uri_string (GstDvswitchUri * uri);
void gst_dvswitch_uri_free (GstDvswitchUri * uri);
int gst_dvswitch_uri_update (GstDvswitchUri * uri, const gchar * host, gint port);
static const gchar * gst_dvswitch_src_uri_get_uri (GstURIHandler * handler);


/* initialize the dvswitchsrc's class */
static void
gst_dvswitch_src_class_init (GstDvswitchSrcClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_metadata(element_class,
    "dvswitch source",
    "Source/Video",
    "Reads DIF/DV stream from dvswitch server.",
    "Michael Farrell <michael@uanywhere.com.au>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));

  GObjectClass *gobject_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstPushSrcClass *gstpushsrc_class;

  gobject_class = (GObjectClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstpushsrc_class = (GstPushSrcClass *) klass;

  /* define virtual function pointers */

  gobject_class->set_property = gst_dvswitch_src_set_property;
  gobject_class->get_property = gst_dvswitch_src_get_property;
  gobject_class->finalize = gst_dvswitch_src_finalize;

  /* define properties */

  g_object_class_install_property (gobject_class, PROP_HOSTNAME,
      g_param_spec_string ("hostname", "Hostname", "Hostname of dvswitch server.",
          DVSWITCH_DEFAULT_HOSTNAME, G_PARAM_READWRITE));
   
  g_object_class_install_property (gobject_class, PROP_PORT,
      g_param_spec_int ("port", "Port", "Port of dvswitch server.", 0, 65535,
          DVSWITCH_DEFAULT_PORT, G_PARAM_READWRITE));
          
  g_object_class_install_property (gobject_class, PROP_URI,
      g_param_spec_string("uri", "URI", "URI in the form of dvswitch://ip:port",
      	DVSWITCH_DEFAULT_URI, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_BUFFER_SIZE,
      g_param_spec_int ("buffer-size", "Buffer Size",
          "Size of the kernel receive buffer in bytes, 0=default", 0, G_MAXINT,
          DVSWITCH_DEFAULT_BUFFER_SIZE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
          
  g_object_class_install_property (gobject_class, PROP_TIMEOUT,
      g_param_spec_uint64 ("timeout", "Timeout",
          "Post a message after timeout microseconds (0 = disabled)", 0,
          G_MAXUINT64, DVSWITCH_DEFAULT_TIMEOUT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
          
  gstbasesrc_class->start = gst_dvswitch_src_start;
  gstbasesrc_class->stop = gst_dvswitch_src_stop;
  gstbasesrc_class->unlock = gst_dvswitch_src_unlock;
  gstbasesrc_class->unlock_stop = gst_dvswitch_src_unlock_stop;
  gstbasesrc_class->get_caps = gst_dvswitch_src_get_caps;
  gstpushsrc_class->create = gst_dvswitch_src_create;
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_dvswitch_src_init (GstDvswitchSrc * filter)
{
/*
  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_set_getcaps_function (filter->srcpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);
  */
  
  gst_dvswitch_uri_init(&filter->uri, DVSWITCH_DEFAULT_HOSTNAME, DVSWITCH_DEFAULT_PORT);
/*  filter->hostname = DVSWITCH_DEFAULT_HOSTNAME;
  filter->port = DVSWITCH_DEFAULT_PORT;
  */
 /* configure basesrc to be a live source */
  gst_base_src_set_live (GST_BASE_SRC (filter), TRUE);
  /* make basesrc output a segment in time */
  gst_base_src_set_format (GST_BASE_SRC (filter), GST_FORMAT_TIME);
  /* make basesrc set timestamps on outgoing buffers based on the running_time
   * when they were captured */
  gst_base_src_set_do_timestamp (GST_BASE_SRC (filter), TRUE);
  
}

static void
gst_dvswitch_src_finalize (GObject * object)
{
  GstDvswitchSrc *dvsrc;

  dvsrc = GST_DVSWITCHSRC (object);

  gst_dvswitch_uri_free (&dvsrc->uri);
  g_free (dvsrc->uristr);

  if (dvsrc->sockfd >= 0)
    CLOSE_SOCKET (dvsrc->sockfd);

  WSA_CLEANUP (object);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
gst_dvswitch_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDvswitchSrc *filter = GST_DVSWITCHSRC (object);
  const gchar *hostname;

  switch (prop_id) {
    case PROP_HOSTNAME:
      //filter->hostname = g_value_dup_string (value);
      if ((hostname = g_value_get_string (value)))
      	gst_dvswitch_uri_update(&filter->uri, hostname, -1);
     	else
     		gst_dvswitch_uri_update(&filter->uri, DVSWITCH_DEFAULT_HOSTNAME, -1);
      break;
    case PROP_URI:
    	gst_dvswitch_src_set_uri(filter, g_value_get_string(value));
    	break;
    case PROP_PORT:
    	//filter->port = g_value_get_uint (value);
    	gst_dvswitch_uri_update(&filter->uri, NULL, g_value_get_int(value));
    	break;
    case PROP_BUFFER_SIZE:
      filter->buffer_size = g_value_get_int (value);
      break;
    case PROP_TIMEOUT:
      filter->timeout = g_value_get_uint64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_dvswitch_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDvswitchSrc *filter = GST_DVSWITCHSRC (object);

  switch (prop_id) {
    case PROP_HOSTNAME:
      g_value_set_string (value, filter->uri.host);
      break;
    case PROP_PORT:
      g_value_set_int (value, filter->uri.port);
      break;
    case PROP_BUFFER_SIZE:
      g_value_set_int (value, filter->buffer_size);
      break;
    case PROP_TIMEOUT:
      g_value_set_uint64 (value, filter->timeout);
      break;
    case PROP_URI:
      g_value_set_string (value, gst_dvswitch_src_uri_get_uri(GST_URI_HANDLER(filter)));
      break;  
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

// pretty much taken from gst_dvswitchsrc_create
static GstFlowReturn
gst_dvswitch_src_create (GstPushSrc * psrc, GstBuffer ** buf)
{
  GstDvswitchSrc *dvswitchsrc;
  union gst_sockaddr
  {
    struct sockaddr sa;
    struct sockaddr_in sa_in;
    struct sockaddr_in6 sa_in6;
    struct sockaddr_storage sa_stor;
  } sa;
  socklen_t slen;
  guint8 *pktdata;
  gint pktsize;
#ifdef G_OS_UNIX
  gint readsize;
#elif defined G_OS_WIN32
  gulong readsize;
#endif
  GstClockTime timeout;
  gint ret;
  gboolean try_again;
  GstBuffer *outbuf;

  dvswitchsrc = GST_DVSWITCHSRC (psrc);

retry:
  /* quick check, avoid going in select when we already have data */
  readsize = 0;
  if (G_UNLIKELY ((ret =
              IOCTL_SOCKET (dvswitchsrc->sock.fd, FIONREAD, &readsize)) < 0))
    goto ioctl_failed;

  if (readsize > 0)
    goto no_select;

  if (dvswitchsrc->timeout > 0) {
    timeout = dvswitchsrc->timeout * GST_USECOND;
  } else {
    timeout = GST_CLOCK_TIME_NONE;
  }

  do {
    try_again = FALSE;

    GST_LOG_OBJECT (dvswitchsrc, "doing select, timeout %" G_GUINT64_FORMAT,
        dvswitchsrc->timeout);

    ret = gst_poll_wait (dvswitchsrc->fdset, timeout);
    GST_LOG_OBJECT (dvswitchsrc, "select returned %d", ret);
    if (G_UNLIKELY (ret < 0)) {
      if (errno == EBUSY)
        goto stopped;
#ifdef G_OS_WIN32
      if (WSAGetLastError () != WSAEINTR)
        goto select_error;
#else
      if (errno != EAGAIN && errno != EINTR)
        goto select_error;
#endif
      try_again = TRUE;
    } else if (G_UNLIKELY (ret == 0)) {
      /* timeout, post element message */
      gst_element_post_message (GST_ELEMENT_CAST (dvswitchsrc),
          gst_message_new_element (GST_OBJECT_CAST (dvswitchsrc),
              gst_structure_new ("GstDvswitchTimeout",
                  "timeout", G_TYPE_UINT64, dvswitchsrc->timeout, NULL)));
      try_again = TRUE;
    }
  } while (G_UNLIKELY (try_again));

  /* ask how much is available for reading on the socket, this should be exactly
   * one UDP packet. We will check the return value, though, because in some
   * case it can return 0 and we don't want a 0 sized buffer. */
  readsize = 0;
  if (G_UNLIKELY ((ret =
              IOCTL_SOCKET (dvswitchsrc->sock.fd, FIONREAD, &readsize)) < 0))
    goto ioctl_failed;

  /* If we get here and the readsize is zero, then either select was woken up
   * by activity that is not a read, or a poll error occurred, or a UDP packet
   * was received that has no data. Since we cannot identify which case it is,
   * we handle all of them. This could possibly lead to a UDP packet getting
   * lost, but since UDP is not reliable, we can accept this. */
  if (G_UNLIKELY (!readsize)) {
    /* try to read a packet (and it will be ignored),
     * in case a packet with no data arrived */
    slen = sizeof (sa);
    recvfrom (dvswitchsrc->sock.fd, (char *) &slen, 0, 0, &sa.sa, &slen);

    /* clear any error, in case a poll error occurred */
    //clear_error (dvswitchsrc);

    /* poll again */
    goto retry;
  }

no_select:
  GST_LOG_OBJECT (dvswitchsrc, "ioctl says %d bytes available", (int) readsize);

  pktdata = g_malloc (readsize);
  pktsize = readsize;

  while (TRUE) {
    slen = sizeof (sa);
#ifdef G_OS_WIN32
    ret = recvfrom (dvswitchsrc->sock.fd, (char *) pktdata, pktsize, 0, &sa.sa,
        &slen);
#else
    ret = recvfrom (dvswitchsrc->sock.fd, pktdata, pktsize, 0, &sa.sa, &slen);
#endif
    if (G_UNLIKELY (ret < 0)) {
#ifdef G_OS_WIN32
      /* WSAECONNRESET for a UDP socket means that a packet sent with udpsink
       * generated a "port unreachable" ICMP response. We ignore that and try
       * again. */
      if (WSAGetLastError () == WSAECONNRESET) {
        g_free (pktdata);
        pktdata = NULL;
        goto retry;
      }
      if (WSAGetLastError () != WSAEINTR)
        goto receive_error;
#else
      if (errno != EAGAIN && errno != EINTR)
        goto receive_error;
#endif
    } else
      break;
  }
  GST_LOG_OBJECT (dvswitchsrc, "read %d bytes", (int) readsize);
  
  outbuf = gst_buffer_new ();

  // ret is size of pktdata

  gst_buffer_insert_memory (outbuf, -1,
                            gst_memory_new_wrapped (0, pktdata,
                                    ret, 0, ret, pktdata, g_free));

  *buf = outbuf;

  return GST_FLOW_OK;

  /* ERRORS */
select_error:
  {
    GST_ELEMENT_ERROR (dvswitchsrc, RESOURCE, READ, (NULL),
        ("select error %d: %s (%d)", ret, g_strerror (errno), errno));
    return GST_FLOW_ERROR;
  }
stopped:
  {
    GST_DEBUG ("stop called");
    return GST_FLOW_FLUSHING;
  }
ioctl_failed:
  {
    GST_ELEMENT_ERROR (dvswitchsrc, RESOURCE, READ, (NULL),
        ("ioctl failed %d: %s (%d)", ret, g_strerror (errno), errno));
    return GST_FLOW_ERROR;
  }
receive_error:
  {
    g_free (pktdata);
#ifdef G_OS_WIN32
    GST_ELEMENT_ERROR (dvswitchsrc, RESOURCE, READ, (NULL),
        ("receive error %d (WSA error: %d)", ret, WSAGetLastError ()));
#else
    GST_ELEMENT_ERROR (dvswitchsrc, RESOURCE, READ, (NULL),
        ("receive error %d: %s (%d)", ret, g_strerror (errno), errno));
#endif
    return GST_FLOW_ERROR;
  }
}


/* create a socket for sending to remote machine */
static gboolean
gst_dvswitch_src_start (GstBaseSrc * bsrc)
{
  GstDvswitchSrc *src;

  src = GST_DVSWITCHSRC (bsrc);
  
  
	struct addrinfo addr_hints = {
		.ai_family =   AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
		.ai_flags =    AI_ADDRCONFIG
	};
	struct addrinfo * addr;
	int error;
	
	char port[NI_MAXSERV];
	memset(port, 0, sizeof(port));
	g_snprintf(port, sizeof(port) - 1, "%d", src->uri.port);
	port[sizeof(port)-1] = '\0';
	
	if ((error = getaddrinfo(src->uri.host, port, &addr_hints, &addr)))
		goto getaddrinfo_error;
	

	// FIXME: this should probably try other addresses too, not only the first.
	src->sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	if (src->sockfd < 0)
		goto no_socket;
		
	if (connect(src->sockfd, addr->ai_addr, addr->ai_addrlen) != 0)
		goto connect_error;
	
	freeaddrinfo(addr);
	
	src->sock.fd = src->sockfd;
	
	// send command to tell dvswitch to start dumping frames at us
	if (write(src->sockfd, GREETING_RAW_SINK, GREETING_SIZE) != GREETING_SIZE)
		goto write_error;

  if ((src->fdset = gst_poll_new (TRUE)) == NULL)
    goto no_fdset;

  gst_poll_add_fd (src->fdset, &src->sock);
  gst_poll_fd_ctl_read (src->fdset, &src->sock, TRUE);

  return TRUE;

  /* ERRORS */
getaddrinfo_error:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS, (NULL),
        ("getaddrinfo failed: %s (%d)", gai_strerror (error), error));
    return FALSE;
  }
no_socket:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
        ("no socket error %d: %s (%d)", error, g_strerror (errno), errno));
    return FALSE;
  }
connect_error:
  {
    CLOSE_IF_REQUESTED (src);
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS, (NULL),
        ("connect failed %d: %s (%d) to %s:%d", error, g_strerror (errno), errno, src->uri.host, src->uri.port));
    return FALSE;
  }
write_error:
  {
    CLOSE_IF_REQUESTED (src);
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS, (NULL),
        ("write failed %d: %s (%d)", error, g_strerror (errno), errno));
    return FALSE;
  }
no_fdset:
  {
    CLOSE_IF_REQUESTED (src);
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ_WRITE, (NULL),
        ("could not create an fdset %d: %s (%d)", error, g_strerror (errno),
            errno));
    return FALSE;
  }
}

static gboolean
gst_dvswitch_src_unlock (GstBaseSrc * bsrc)
{
  GstDvswitchSrc *src;

  src = GST_DVSWITCHSRC (bsrc);

  GST_LOG_OBJECT (src, "Flushing");
  gst_poll_set_flushing (src->fdset, TRUE);

  return TRUE;
}

static gboolean
gst_dvswitch_src_unlock_stop (GstBaseSrc * bsrc)
{
  GstDvswitchSrc *src;

  src = GST_DVSWITCHSRC (bsrc);

  GST_LOG_OBJECT (src, "No longer flushing");
  gst_poll_set_flushing (src->fdset, FALSE);

  return TRUE;
}

static gboolean
gst_dvswitch_src_stop (GstBaseSrc * bsrc)
{
  GstDvswitchSrc *src;

  src = GST_DVSWITCHSRC (bsrc);

  GST_DEBUG ("stopping, closing sockets");

  if (src->sock.fd >= 0) {
    CLOSE_IF_REQUESTED (src);
  }

  if (src->fdset) {
    gst_poll_free (src->fdset);
    src->fdset = NULL;
  }

  return TRUE;
}

static GstCaps *
gst_dvswitch_src_get_caps (GstBaseSrc * src, GstCaps * filter)
{
    return gst_caps_new_any ();
}

static gboolean
gst_dvswitch_src_set_uri (GstDvswitchSrc * src, const gchar * uri)
{
  if (gst_dvswitch_parse_uri (uri, &src->uri) < 0)
    goto wrong_uri;

  if (src->uri.port == -1)
    src->uri.port = DVSWITCH_DEFAULT_PORT;

  return TRUE;

  /* ERRORS */
wrong_uri:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
        ("error parsing uri %s", uri));
    return FALSE;
  }
}


/*** GSTURIHANDLER INTERFACE *************************************************/

static GstURIType
gst_dvswitch_src_uri_get_type (GType type)
{
  return GST_URI_SRC;
}

static gchar **
gst_dvswitch_src_uri_get_protocols (GType type)
{
  static gchar *protocols[] = { (char *) "dvswitch", NULL };

  return protocols;
}

static const gchar *
gst_dvswitch_src_uri_get_uri (GstURIHandler * handler)
{
  GstDvswitchSrc *src = GST_DVSWITCHSRC (handler);

  g_free (src->uristr);
  src->uristr = gst_dvswitch_uri_string (&src->uri);

  return src->uristr;
}

static gboolean
gst_dvswitch_src_uri_set_uri (GstURIHandler * handler, const gchar * uri, GError **error)
{
  gboolean ret;

  GstDvswitchSrc *src = GST_DVSWITCHSRC (handler);

  ret = gst_dvswitch_src_set_uri (src, uri);

  return ret;
}

static void
gst_dvswitch_src_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_dvswitch_src_uri_get_type;
  iface->get_protocols = gst_dvswitch_src_uri_get_protocols;
  iface->get_uri = gst_dvswitch_src_uri_get_uri;
  iface->set_uri = gst_dvswitch_src_uri_set_uri;
}

void
gst_dvswitch_uri_init (GstDvswitchUri * uri, const gchar * host, gint port)
{
  uri->host = NULL;
  uri->port = -1;
  gst_dvswitch_uri_update (uri, host, port);
}

int
gst_dvswitch_uri_update (GstDvswitchUri * uri, const gchar * host, gint port)
{
  if (host) {
    g_free (uri->host);
    uri->host = g_strdup (host);
    if (strchr (host, ':'))
      uri->is_ipv6 = TRUE;
    else
      uri->is_ipv6 = FALSE;
  }
  if (port != -1)
    uri->port = port;

  return 0;
}

int
gst_dvswitch_parse_uri (const gchar * uristr, GstDvswitchUri * uri)
{
  gchar *protocol;
  gchar *location, *location_end;
  gchar *colptr;

  /* consider no protocol to be dvswitch:// */
  protocol = gst_uri_get_protocol (uristr);
  if (!protocol)
    goto no_protocol;
  if (strcmp (protocol, "dvswitch") != 0)
    goto wrong_protocol;
  g_free (protocol);

  location = gst_uri_get_location (uristr);
  if (!location)
    return FALSE;

  if (location[0] == '[') {
    GST_DEBUG ("parse IPV6 address '%s'", location);
    location_end = strchr (location, ']');
    if (location_end == NULL)
      goto wrong_address;

    uri->is_ipv6 = TRUE;
    g_free (uri->host);
    uri->host = g_strndup (location + 1, location_end - location - 1);
    colptr = strrchr (location_end, ':');
  } else {
    GST_DEBUG ("parse IPV4 address '%s'", location);
    uri->is_ipv6 = FALSE;
    colptr = strrchr (location, ':');

    g_free (uri->host);
    if (colptr != NULL) {
      uri->host = g_strndup (location, colptr - location);
    } else {
      uri->host = g_strdup (location);
    }
  }
  GST_DEBUG ("host set to '%s'", uri->host);

  if (colptr != NULL) {
    uri->port = atoi (colptr + 1);
  }
  g_free (location);

  return 0;

  /* ERRORS */
no_protocol:
  {
    GST_ERROR ("error parsing uri %s: no protocol", uristr);
    return -1;
  }
wrong_protocol:
  {
    GST_ERROR ("error parsing uri %s: wrong protocol (%s != dvswitch)", uristr,
        protocol);
    g_free (protocol);
    return -1;
  }
wrong_address:
  {
    GST_ERROR ("error parsing uri %s", uristr);
    g_free (location);
    return -1;
  }
}

gchar *
gst_dvswitch_uri_string (GstDvswitchUri * uri)
{
  gchar *result;

  if (uri->is_ipv6) {
    result = g_strdup_printf ("dvswitch://[%s]:%d", uri->host, uri->port);
  } else {
    result = g_strdup_printf ("dvswitch://%s:%d", uri->host, uri->port);
  }
  return result;
}

void
gst_dvswitch_uri_free (GstDvswitchUri * uri)
{
  g_free (uri->host);
  uri->host = NULL;
  uri->port = -1;
}

