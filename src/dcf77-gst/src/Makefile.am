plugin_LTLIBRARIES = libdcf77.la

# sources used to compile this plug-in
libdcf77_la_SOURCES = dcf77.c

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libdcf77_la_CFLAGS = $(GST_CFLAGS)
libdcf77_la_LIBADD = $(GST_LIBS)
libdcf77_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

gstplugindir = $(includedir)/gstreamer-$(GST_MAJORMINOR)/gst
gstplugin_HEADERS = dcf77.h
