# Note: plugindir is set in configure

##############################################################################
# TODO: change libgstdvswitch.la to something else, e.g. libmysomething.la     #
##############################################################################
plugin_LTLIBRARIES = libgstdvswitch.la

##############################################################################
# TODO: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

# sources used to compile this plug-in
libgstdvswitch_la_SOURCES = gstdvswitchsrc.c gstdvswitchsrc.h gstdvswitchsink.c plugin.c

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstdvswitch_la_CFLAGS = $(GST_CFLAGS)
libgstdvswitch_la_LIBADD = $(GST_LIBS)
libgstdvswitch_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstdvswitch_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstdvswitchsrc.h gstdvswitchsink.h
