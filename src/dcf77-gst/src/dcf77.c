/*
 * U77,5 - a set of USB sound / DCF77 tools
 * Copyright (C) 2008 Detlef Reichl <detlef!reichl()gmx!org>
 *                    Hayati Ayguen <h_ayguen@web.de>
 * License: GNU LGPL (GNU Lesser General Public License, see COPYING)
 *
 * This file is part of U77,5.
 *
 * U77,5 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * U77,5 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser Public License for more details.
 *
 * You should have received a copy of the GNU Lesser Public License
 * along with U77,5.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <math.h>

#include <string.h>
#include <gst/gst.h>

#include "dcf77.h"

GST_DEBUG_CATEGORY_STATIC (dcf77_debug);
#define GST_CAT_DEFAULT dcf77_debug

enum
{
  SIGNAL_MINUTE_PULSE,
  LAST_SIGNAL
};

enum
{
  ARG_0,
  ARG_SILENT,
  ARG_VERBOSE_TIME_EVAL
};

static guint dcf77_signals[LAST_SIGNAL] = {0};


static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS (
    "audio/x-raw-int, "
    "width = (int) 16, "
    "depth = (int) 16, "
    "endianess = (int) BYTE_ORDER, "
    "channels = (int) {1}, "
    "rate = (int) {48000}"
  )
);


GST_BOILERPLATE (Dcf77, dcf77, GstElement, GST_TYPE_ELEMENT);


static void            dcf77_set_property    (GObject * object,
                                              guint prop_id,
                                              const GValue * value,
                                              GParamSpec * pspec);
static void            dcf77_get_property    (GObject * object,
                                              guint prop_id,
                                              GValue * value,
                                              GParamSpec * pspec);
static gboolean        dcf77_set_caps        (GstPad * pad,
                                              GstCaps * caps);
static GstFlowReturn   dcf77_chain           (GstPad * pad,
                                              GstBuffer * buf);

static void            init_get_threshold    (Dcf77 *filter);
static void            init_get_time         (Dcf77 *filter);
static const gchar    *eval_min_pulse        (Dcf77 *filter);

void                   dcf77_minute_pulse    (Dcf77 *filter);


static void
dcf77_base_init (gpointer gclass)
{
  static GstElementDetails element_details = {
    "DCF77 decoder",
    "Analyzer/Audio/DCF77",
    "Decodes an incoming DCF77 audio stream, for date and time information.",
    "Detlef Reichl <detlef ! reichl () gmx ! org>"
  };
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_add_pad_template (element_class,
                                      gst_static_pad_template_get (&sink_factory));
  gst_element_class_set_details (element_class, &element_details);
}


static void
dcf77_class_init (Dcf77Class * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = dcf77_set_property;
  gobject_class->get_property = dcf77_get_property;
 
  dcf77_signals[SIGNAL_MINUTE_PULSE] = g_signal_new ("minute-pulse",
                                                  G_TYPE_FROM_CLASS (klass),
                                                  G_SIGNAL_RUN_LAST,
                                                  G_STRUCT_OFFSET (Dcf77Class, minute_pulse),
                                                  NULL, NULL,
                                                  g_cclosure_marshal_VOID__VOID,
                                                  G_TYPE_NONE,
                                                  0);
  
  

  g_object_class_install_property (gobject_class,
                                   ARG_SILENT,
                                   g_param_spec_boolean ("silent",
                                                         "Silent",
                                                         "Produce verbose output ?",
                                                          FALSE,
                                                          G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   ARG_VERBOSE_TIME_EVAL,
                                   g_param_spec_boolean ("verbose time eval",
                                                         "Verbose time eval",
                                                         "Should the time evaluation print warnings if it fails?",
                                                          FALSE,
                                                          G_PARAM_READWRITE));
}


static void
dcf77_init (Dcf77 *filter,
            Dcf77Class *gclass)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (filter);

  filter->sinkpad = gst_pad_new_from_template (gst_element_class_get_pad_template (klass, "sink"),
                                               "sink");
  gst_pad_set_setcaps_function (filter->sinkpad,
                                GST_DEBUG_FUNCPTR(dcf77_set_caps));
  gst_pad_set_getcaps_function (filter->sinkpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
  gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(dcf77_chain));

  filter->tms_is_valid = FALSE;
  filter->silent = FALSE;
  filter->verbose = FALSE;
  init_get_threshold (filter);
}

static void
dcf77_set_property (GObject * object,
                    guint prop_id,
                    const GValue * value,
                    GParamSpec * pspec)
{
  Dcf77 *filter = DCF77 (object);

  switch (prop_id) {
    case ARG_SILENT:
      filter->silent = g_value_get_boolean (value);
    break;
    case ARG_VERBOSE_TIME_EVAL:
      filter->verbose = g_value_get_boolean (value);
    break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
dcf77_get_property (GObject * object,
                    guint prop_id,
                    GValue * value,
                    GParamSpec * pspec)
{
  Dcf77 *filter = DCF77 (object);

  switch (prop_id) {
    case ARG_SILENT:
      g_value_set_boolean (value, filter->silent);
    break;
    case ARG_VERBOSE_TIME_EVAL:
      g_value_set_boolean (value, filter->verbose);
    break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}



static gboolean
dcf77_set_caps (GstPad * pad, GstCaps * caps)
{
/*
  we don't have to check our self, cause the gst main class does it for us
  
  GstStructure *structure = gst_caps_get_structure (caps, 0);
  const gchar *mime;
  int samplerate;

  mime = gst_structure_get_name (structure);

  if (strcmp (mime, "audio/x-raw-int") != 0)
  {
    GST_WARNING ("Wrong mime type");
    return FALSE;
  }
  gst_structure_get_int (structure, "rate", &samplerate);
  if (samplerate != 48000)
  {
    GST_WARNING ("Unsupported samplerate");
    return FALSE; 
  }
  */

  return gst_pad_set_caps (pad, caps);
}

static GstFlowReturn
dcf77_chain (GstPad * pad, GstBuffer * buf)
{
  Dcf77 *filter;
  gint16 *data;
  gint32 samples;
  gint16 *sample;
  gboolean high_level;
  gint32 i;
  gboolean resync;
  const gchar *eval_retval;

  filter = DCF77 (GST_OBJECT_PARENT (pad));
  data = (gint16 *) GST_BUFFER_DATA (buf);
  samples = GST_BUFFER_SIZE (buf) / 2;
  sample = data;  

  switch (filter->state)
  {
    case DCF77_STATE_SLEEP:
      gst_buffer_unref (buf);
      return GST_FLOW_OK;
    break;
    case DCF77_STATE_CALIBRATE:
      if (!filter->calibration_cycles)
        filter->calibration_cycles = 3 * 48000;
      filter->calibration_cycles -= samples;

      while (samples--)
      {
        if (filter->calibration_cycles <= 48000)
          filter->calibration_mean += abs (*sample);
        
        if (*sample > filter->calibration_highest)
          filter->calibration_highest = *sample;
        sample++;
      }

      if (filter->calibration_cycles <= 0)
      {
        filter->threshold = filter->calibration_highest * 0.7;
        init_get_time (filter);
      }
    break;


    case DCF77_STATE_GET_TIME:
      resync = FALSE;

      if (filter->frames_since_last_min_pulse >= 0)
        filter->frames_since_last_min_pulse += samples;

      for (i = 0; i < (samples - (samples % 4)); i += 4)
      {
        high_level = (*sample >= filter->threshold
                     || *(sample + 1) >= filter->threshold
                     || *(sample + 2) >= filter->threshold
                     || *(sample + 3) >= filter->threshold) ? TRUE : FALSE;
        if (high_level != filter->last_level)
        {
          const float ms_since_last_pulse = (float)((filter->frames_since_last_pulse + i) * 1000.0 / 48000);
          
          filter->last_level = high_level;          
          
          if (  (-1 == filter->last_bit && ms_since_last_pulse >  60.0 && ms_since_last_pulse < 140.0)  /* ~ 100 ms */
              ||(-1 == filter->last_bit && ms_since_last_pulse > 160.0 && ms_since_last_pulse < 240.0)  /* ~ 200 ms */
             )
          {
            filter->last_bit = (ms_since_last_pulse < 150.0) ? 0 : 1;
            /* DCF bits 28 .. 0: add 1 new bit from Hi and shift old bits */
            filter->value_mask_low = ((filter->value_mask_high & 1) << 28) | (filter->value_mask_low >> 1);
            filter->valid_mask_low = ((filter->valid_mask_high & 1) << 28) | (filter->valid_mask_low >> 1);
            /* DCF bits 58 .. 29 == 29 .. 0: add 1 new bit and shift old bits */
            filter->value_mask_high = (filter->last_bit << 29) | (filter->value_mask_high >> 1);
            filter->valid_mask_high = (1 << 29) | (filter->valid_mask_high >> 1);

            filter->diff_frames[filter->diff_index] = filter->frames_since_last_pulse + i;
            filter->diff_index = 1 - filter->diff_index;
            filter->frames_since_last_pulse = - (int)i;
          }
          else if (  (0 == filter->last_bit && ms_since_last_pulse > 860.0 && ms_since_last_pulse < 940.0)  /* ~ 900 ms */
                   ||(1 == filter->last_bit && ms_since_last_pulse > 760.0 && ms_since_last_pulse < 840.0)  /* ~ 800 ms */
                  )
          {
            filter->last_bit = -1;   /* after 100 ms or 200 ms Pulse at Second pulse */

            filter->diff_frames[filter->diff_index] = filter->frames_since_last_pulse + i;
            filter->diff_index = 1 - filter->diff_index;
            filter->frames_since_last_pulse = - i;
          }
          else if (  (0 == filter->last_bit && ms_since_last_pulse > 1860.0 && ms_since_last_pulse < 1940.0)  /* ~ 1900 ms */
                   ||(1 == filter->last_bit && ms_since_last_pulse > 1760.0 && ms_since_last_pulse < 1840.0)  /* ~ 1800 ms */
                  )
          {
            filter->last_bit = -1; /* after 100 ms or 200 ms Pulse at Minute pulse */
            filter->frames_since_last_min_pulse = 48000 - i;
            filter->eval_value_mask_low = filter->value_mask_low;
            filter->eval_valid_mask_low = filter->valid_mask_low;
            filter->eval_value_mask_high = filter->value_mask_high;
            filter->eval_valid_mask_high = filter->valid_mask_high;
            filter->diff_frames[filter->diff_index] = filter->frames_since_last_pulse + i;
            filter->diff_index = 1 - filter->diff_index;
            filter->frames_since_last_pulse = - i;
            
            eval_retval = eval_min_pulse (filter);
            if (eval_retval && filter->verbose)
              g_message ("%s", eval_retval);
            else
            {
//              printf ("%i:%i\n", filter->tms.tm_hour, filter->tms.tm_min);
              g_signal_emit (G_OBJECT (filter), dcf77_signals[SIGNAL_MINUTE_PULSE], 0);
            }
          }
          else if (ms_since_last_pulse >= 20000.0 && ms_since_last_pulse < 50000.0)  /* initial pulse search? */
          {
            filter->last_bit = -1;   /* ignore */
            filter->frames_since_last_pulse = - i;
          }
          else if ( -1 == filter->last_bit && ms_since_last_pulse < 30.0 )
          {
            /* filter noise! */
            filter->last_bit = -1;
          }
          else
          {
            resync = TRUE;
            filter->last_bit = -1;
            filter->frames_since_last_pulse = - i;
          }
        }
        sample += 4;
      } /* end for */
      filter->frames_since_last_pulse += samples;

      if (( filter->frames_since_last_pulse > 10.0 * 48000
          && filter->frames_since_last_pulse < 20.0 * 48000)
          || resync
         )
      {
        init_get_threshold(filter);
      }
    break;
    case DCF77_STATE_ANALYSE:
    break;
    default:
      g_assert_not_reached();
  }
  gst_buffer_unref (buf);
  return GST_FLOW_OK;
}


static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (dcf77_debug, "dcf77", 0, "DCF77 decoder");

  return gst_element_register (plugin, "dcf77", GST_RANK_NONE, TYPE_DCF77);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "dcf77",
    "DCF77 decoder",
    plugin_init,
    VERSION,
    "LGPL",
    PACKAGE,
    "http://u775.sourceforge.net");
    





static void
init_get_threshold (Dcf77 *filter)
{
  filter->state = DCF77_STATE_CALIBRATE;
  filter->calibration_cycles = 0;
  filter->calibration_highest = 0;
  filter->last_rising_edge = 0;
  filter->calibration_mean = 0;
}

static void
init_get_time (Dcf77 *filter)
{
  filter->state = DCF77_STATE_GET_TIME;
  filter->frame_index = 0;

  filter->diff_frames[0] = 0;
  filter->diff_frames[1] = 0;
  
  filter->diff_index = 0;
  filter->last_level = FALSE;
  filter->frames_since_last_min_pulse = -1;
  filter->frames_since_last_pulse = 21 * 48000;

  filter->last_bit = -1;

  filter->value_mask_low = 0;
  filter->valid_mask_low = 0;
  filter->value_mask_high = 0;
  filter->valid_mask_high = 0;

  filter->eval_value_mask_low = 0;
  filter->eval_valid_mask_low = 0;
  filter->eval_value_mask_high = 0;
  filter->eval_valid_mask_high = 0;
}



static const gchar *
eval_min_pulse(Dcf77 *filter)
{
  // Lo: 28 .. 0
  // 22222222211111111110000000000
  // 87654321098765432109876543210
  // 11111111101110000000000000000 == 0x1FF70000 == ValidMaskLo
  // 00000000100000000000000000000 ==   0x100000 == ValueMaskLo (== Startbit 20)
  // Hi: 58 .. 29
  // 555555555444444444433333333332
  // 876543210987654321098765432109
  // 111111111111111111111111111111 == 0x3FFFFFFF == ValidMaskHi

  //int DST_change    = ( (filter->eval_value_mask_low >> 16) & 0x01);
//  int TimeZone      = ( (filter->eval_value_mask_low >> 17) & 0x03);


  guint8 MinuteBCDLo, MinuteBCDHi, ParityMinute, Minute;
  guint8 HourBCDLo, HourBCDHi, ParityHour, Hour;
  guint8 DayBCDLo, DayBCDHi, Day;
  guint8 Weekday;
  guint8 MonthBCDLo, MonthBCDHi, Month;
  guint8 YearBCDLo, YearBCDHi, Year;
  guint8 Century = 20;
  guint8 ParityDate;

  filter->tms_is_valid = FALSE;

  MinuteBCDLo   = ( (filter->eval_value_mask_low >> 21) & 0x0f);
  MinuteBCDHi   = ( (filter->eval_value_mask_low >> 25) & 0x07);
  ParityMinute  = ( (filter->eval_value_mask_low >> 21)  // 21 Start Minute
                   ^ (filter->eval_value_mask_low >> 22)  // 22
                   ^ (filter->eval_value_mask_low >> 23)  // 23
                   ^ (filter->eval_value_mask_low >> 24)  // 24
                   ^ (filter->eval_value_mask_low >> 25)  // 25
                   ^ (filter->eval_value_mask_low >> 26)  // 26
                   ^ (filter->eval_value_mask_low >> 27)  // 27 End Minute
                   ^ (filter->eval_value_mask_low >> 28)  // 28 Parity Minute
                  ) & 1;
  Minute = MinuteBCDLo + 10 * MinuteBCDHi;

  HourBCDLo     = ( (filter->eval_value_mask_high      ) & 0x0f);
  HourBCDHi     = ( (filter->eval_value_mask_high >>  4) & 0x03);
  ParityHour    = ( (filter->eval_value_mask_high)        // 29 Start Hour
                   ^ (filter->eval_value_mask_high >>  1)  // 30
                   ^ (filter->eval_value_mask_high >>  2)  // 31
                   ^ (filter->eval_value_mask_high >>  3)  // 32
                   ^ (filter->eval_value_mask_high >>  4)  // 33
                   ^ (filter->eval_value_mask_high >>  5)  // 34 End Hour
                   ^ (filter->eval_value_mask_high >>  6)  // 35 Parity Hour
                  ) & 1;
  Hour = HourBCDLo + 10 * HourBCDHi;

  DayBCDLo      = ( (filter->eval_value_mask_high >>  7) & 0x0f);
  DayBCDHi      = ( (filter->eval_value_mask_high >> 11) & 0x03);
  Weekday       = ( (filter->eval_value_mask_high >> 13) & 0x07);  // 1 .. 7
  MonthBCDLo    = ( (filter->eval_value_mask_high >> 16) & 0x0f);
  MonthBCDHi    = ( (filter->eval_value_mask_high >> 20) & 0x01);
  YearBCDLo     = ( (filter->eval_value_mask_high >> 21) & 0x0f);
  YearBCDHi     = ( (filter->eval_value_mask_high >> 25) & 0x0f);

  Day           = DayBCDLo    + 10 * DayBCDHi;
  Month         = MonthBCDLo  + 10 * MonthBCDHi;
  Century       = 20;
  Year          = YearBCDLo   + 10 * YearBCDHi;
  ParityDate    = ( (filter->eval_value_mask_high >>  7)      // 36 Start Day
                   ^ (filter->eval_value_mask_high >>  8)      // 37
                   ^ (filter->eval_value_mask_high >>  9)      // 38
                   ^ (filter->eval_value_mask_high >> 10)      // 39
                   ^ (filter->eval_value_mask_high >> 11)      // 40
                   ^ (filter->eval_value_mask_high >> 12)      // 41 End Day
                   ^ (filter->eval_value_mask_high >> 13)      // 42 Start Weekday
                   ^ (filter->eval_value_mask_high >> 14)      // 43
                   ^ (filter->eval_value_mask_high >> 15)      // 44 End Weekday
                   ^ (filter->eval_value_mask_high >> 16)      // 45 Start Month
                   ^ (filter->eval_value_mask_high >> 17)      // 46
                   ^ (filter->eval_value_mask_high >> 18)      // 47
                   ^ (filter->eval_value_mask_high >> 19)      // 48
                   ^ (filter->eval_value_mask_high >> 20)      // 49 End Month
                   ^ (filter->eval_value_mask_high >> 21)      // 50 Start Year
                   ^ (filter->eval_value_mask_high >> 22)      // 51
                   ^ (filter->eval_value_mask_high >> 23)      // 52
                   ^ (filter->eval_value_mask_high >> 24)      // 53
                   ^ (filter->eval_value_mask_high >> 25)      // 54
                   ^ (filter->eval_value_mask_high >> 26)      // 55
                   ^ (filter->eval_value_mask_high >> 27)      // 56
                   ^ (filter->eval_value_mask_high >> 28)      // 57 End Year
                   ^ (filter->eval_value_mask_high >> 29)      // 58 Parity Year
                  ) & 1;




  if ((filter->eval_valid_mask_low & 0x1FF70000) != 0x1FF70000
      || (filter->eval_valid_mask_high & 0x3FFFFFFF) != 0x3FFFFFFF)
    return "Error: Not enough bits collected\0";

  if ((filter->eval_value_mask_low & 0x100000) != 0x100000)
    return "Error: Startbit 20 not set\0";


/*  else if (0 == TimeZone || 3 == TimeZone)
  {
    if (filter->verbose)
      g_message  ("Error: TimeZone neither MEZ nor MESZ\n");
  }*/
  if (0 != ParityMinute )
    return "Error: Defect Parity for Minute\0";

  if (MinuteBCDLo > 9)
    return "Error: Lower digit for Minute not in range 0 .. 9\0";

  if (MinuteBCDHi > 5)
   return "Error: Higher digit for Minute not in range 0 .. 5\0";

  if (0 != ParityHour)
    return "Error: Defect Parity for Hour\0";

  if (HourBCDLo > 9)
    return "Error: Lower digit for Hour not in range 0 .. 9\0";

  if (HourBCDHi > 2)
    return "Error: Higher digit for Hour not in range 0 .. 2\0";

  if (Hour > 23)
    return "Error: Hour not in range 0 .. 23\0";

  if (0 != ParityDate)
    return "Error: Defect Parity for Date\0";

  if (DayBCDLo > 9)
    return "Error: Lower digit for Day not in range 0 .. 9\0";

  if (DayBCDHi > 3)
    return "Error: Higher digit for Day not in range 0 .. 2\0";

  if (Day > 31)
    return "Error: Day not in range 1 .. 31\0";

  if (Weekday < 1 || Weekday > 7)
    return "Error: Weekday not in range 1 .. 7\0";

  if (MonthBCDLo > 9)
    return "Error: Lower digit for Month not in range 0 .. 9\0";

  if (MonthBCDHi > 1)
    return "Error: Higher digit for Month not in range 0 .. 1\0";

  if (Month > 12)
    return "Error: Month not in range 1 .. 12\0";

  if (YearBCDLo > 9)
    return "Error: Lower digit for Year not in range 0 .. 9\0";

  if (YearBCDHi > 9)
    return  "Error: Higher digit for Year not in range 0 .. 9\0";

    
  filter->tms.tm_sec   = 0; // 0 .. 59, 60 for leap sec
  filter->tms.tm_min   = Minute; // 0 .. 59
  filter->tms.tm_hour  = Hour; // 0 .. 23
  filter->tms.tm_mday  = Day;  // 1 .. 31
  filter->tms.tm_mon   = Month -1; // 0 .. 11
  filter->tms.tm_year  = Century * 100 + Year - 1900; // years since 1900
  filter->tms.tm_wday  = (Weekday == 7) ? 0 : Weekday; // convert from 1..7 to 0..6
  filter->tms.tm_yday  = 0; // mktime() ignores this
/*    filter->tms.tm_isdst = (1 == TimeZone) ? 1 : 0; //positive if daylight saving time is in effect, zero if it is not, and negative if unknown */
  filter->tms_is_valid = TRUE;
  
  return NULL;
}



