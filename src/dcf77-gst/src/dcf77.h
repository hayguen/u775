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


#ifndef __DCF77_H__
#define __DCF77_H__

#include <time.h>
#include <gst/gst.h>

G_BEGIN_DECLS


#define TYPE_DCF77             (dcf77_get_type())
#define DCF77(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),TYPE_DCF77,Dcf77))
#define DCF77_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),TYPE_DCF77,Dcf77Class))
#define IS_DCF77(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),TYPE_DCF77))
#define IS_DCF77_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),TYPE_DCF77))

typedef struct _Dcf77      Dcf77;
typedef struct _Dcf77Class Dcf77Class;

typedef enum
{
  DCF77_STATE_SLEEP,
  DCF77_STATE_CALIBRATE,
  DCF77_STATE_GET_TIME,
  DCF77_STATE_ANALYSE,
} Dcf77State;


struct _Dcf77
{
  GstElement element;

  GstPad *sinkpad;

  gboolean silent;
  
  gboolean verbose;

  
  gboolean state;
  gint32   calibration_cycles;
  gint32   calibration_highest;
  gint32   calibration_mean;
  gint16   threshold;
  gint32   last_rising_edge;
  
  
  guint32  sample_rate;
  guint32  frame_index;

  guint32  diff_frames[2];
  guint32  diff_index;
  
  gboolean last_level;
  gint32   frames_since_last_min_pulse;
  gint32   frames_since_last_pulse;
  gint8    last_bit;
  
  guint32  value_mask_low;  /* DCF bits 28 .. 0 */
  guint32  valid_mask_low;
  guint32  value_mask_high;  /* DCF bits 58 .. 29 */
  guint32  valid_mask_high;
  
  guint32  eval_value_mask_low;  /* DCF bits 28 .. 0 */
  guint32  eval_valid_mask_low;
  guint32  eval_value_mask_high;  /* DCF bits 58 .. 29 */
  guint32  eval_valid_mask_high;

  struct tm tms;
  gboolean tms_is_valid;
};

struct _Dcf77Class 
{
  GstElementClass parent_class;
  
  void  (*minute_pulse) (Dcf77 *filter);
};

GType dcf77_get_type (void);

G_END_DECLS

#endif /* __GST_DCF77_H__ */
