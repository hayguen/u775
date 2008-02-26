/* compile with

  gcc `pkg-config --cflags --libs gtk+-2.0 gstreamer-0.10 gstreamer-plugins-base-0.10` simple-clock.c -o simple-clock
  
*/


#include <gtk/gtk.h>
#include <gst/dcf77.h>


static void
minute_pulse (Dcf77 *filter, GtkLabel *label)
{
  gchar *time;
  g_message ("pulse");
  
  time = g_strdup_printf ("<big><big>%02i:%02i</big></big>", filter->tms.tm_hour, filter->tms.tm_min);
  gtk_label_set_markup (label, time);

  g_free (time);
}

int
main (int argc, char **argv)
{
  GtkWidget *win;
  GtkWidget *label;

  GstElement *pipeline;
  GstElement *alsasrc;
  GstElement *dcf77;

  gtk_init (&argc, &argv);
  gst_init (&argc, &argv);


  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (win), "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_container_set_border_width (GTK_CONTAINER (win), 24);
  
  
  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), "<big><big>initialize...</big></big>");
  gtk_container_add (GTK_CONTAINER (win), label);
  
  
  pipeline = gst_pipeline_new ("pipeline");
  alsasrc = gst_element_factory_make ("alsasrc", "alsa-source");
  dcf77 =   gst_element_factory_make ("dcf77", "dcf77-sink");
  gst_bin_add_many (GST_BIN (pipeline), alsasrc, dcf77, NULL);
  gst_element_link_many (alsasrc, dcf77, NULL);
  
  g_object_set (G_OBJECT (alsasrc),
                "device", "hw:1",
                NULL);
  g_object_set (G_OBJECT (dcf77),
                "verbose-time-eval", TRUE,
                NULL);
  
  
  
  
  g_signal_connect (G_OBJECT (dcf77), "minute-pulse",
                    G_CALLBACK (minute_pulse), label);
  
  
  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);
  
  
  gtk_widget_show_all (win);
  
  gtk_main ();
  return 0;
}
