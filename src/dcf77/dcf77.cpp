
/*
 * U77,5 - a set of USB sound / DCF77 tools
 * Copyright (C) 2008 Hayati Ayguen <h_ayguen@web.de>
 * License: GNU LGPL (GNU Lesser Public License, see COPYING)
 *
 * This file is part of U77,5.
 *
 * U77,5 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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


#include "dcf77.h"


DCF77::DCF77()
{
  SampleRate = 48000.0;
  eState = STATE_GET_THRESH;
  ChanCount = 1;
  ChanIdx = 0;
  FramesPerBuffer = 480; // = 10 * 48000 / 1000 == 10 ms
  frameIndex = 0;

  aiDiffFrames[0] = aiDiffFrames[1] = 0;
  iDiffIdx = 0;
  PrintDiff = 0;

  SetSysTime = 0;

  initGetThreshold();
}


DCF77::~DCF77()
{
}


void DCF77::initGetThreshold()
{
  eState = STATE_GET_THRESH;
  frameIndex = 0;
  Sum = 0.0;
  Max = -1.0F;
  ThreshStartMessage = true;
  ThreshFinishMessage = false;
}

void DCF77::initGetTime()
{
  frameIndex = 0;
  LastSample = 2.0F;
  FramesSinceLastPulse = (int)( 0.5 + 20.0 * SampleRate );
  FramesSinceLastMinPulse = -1;
  EvaluatedMinPulse = true;
  LastBit = -1;
  ValueMaskLo = 0;
  ValidMaskLo = 0;
  ValueMaskHi = 0;
  ValidMaskHi = 0;

  EvalValueMaskLo = 0;
  EvalValidMaskLo = 0;
  EvalValueMaskHi = 0;
  EvalValidMaskHi = 0;

  eState = STATE_GET_TIME;
}


bool DCF77::evalMinPulse(struct tm * tms, int * DCF_TZ_idx, FILE * errstream)
{
  // assume error
  bool retval = false;

  // Lo: 28 .. 0
  // 22222222211111111110000000000
  // 87654321098765432109876543210
  // 11111111101110000000000000000 == 0x1FF70000 == ValidMaskLo
  // 00000000100000000000000000000 ==   0x100000 == ValueMaskLo (== Startbit 20)
  // Hi: 58 .. 29
  // 555555555444444444433333333332
  // 876543210987654321098765432109
  // 111111111111111111111111111111 == 0x3FFFFFFF == ValidMaskHi

  //int DST_change    = ( (EvalValueMaskLo >> 16) & 0x01);
  int TimeZone      = ( (EvalValueMaskLo >> 17) & 0x03);

  int MinuteBCDLo   = ( (EvalValueMaskLo >> 21) & 0x0f);
  int MinuteBCDHi   = ( (EvalValueMaskLo >> 25) & 0x07);
  int ParityMinute  = ( (EvalValueMaskLo >> 21)  // 21 Start Minute
                      ^ (EvalValueMaskLo >> 22)  // 22
                      ^ (EvalValueMaskLo >> 23)  // 23
                      ^ (EvalValueMaskLo >> 24)  // 24
                      ^ (EvalValueMaskLo >> 25)  // 25
                      ^ (EvalValueMaskLo >> 26)  // 26
                      ^ (EvalValueMaskLo >> 27)  // 27 End Minute
                      ^ (EvalValueMaskLo >> 28)  // 28 Parity Minute
                      ) & 1;
  int Minute = MinuteBCDLo + 10 * MinuteBCDHi;

  int HourBCDLo     = ( (EvalValueMaskHi      ) & 0x0f);
  int HourBCDHi     = ( (EvalValueMaskHi >>  4) & 0x03);
  int ParityHour    = ( (EvalValueMaskHi)        // 29 Start Hour
                      ^ (EvalValueMaskHi >>  1)  // 30
                      ^ (EvalValueMaskHi >>  2)  // 31
                      ^ (EvalValueMaskHi >>  3)  // 32
                      ^ (EvalValueMaskHi >>  4)  // 33
                      ^ (EvalValueMaskHi >>  5)  // 34 End Hour
                      ^ (EvalValueMaskHi >>  6)  // 35 Parity Hour
                      ) & 1;
  int Hour = HourBCDLo + 10 * HourBCDHi;

  int DayBCDLo      = ( (EvalValueMaskHi >>  7) & 0x0f);
  int DayBCDHi      = ( (EvalValueMaskHi >> 11) & 0x03);
  int Weekday       = ( (EvalValueMaskHi >> 13) & 0x07);  // 1 .. 7
  int MonthBCDLo    = ( (EvalValueMaskHi >> 16) & 0x0f);
  int MonthBCDHi    = ( (EvalValueMaskHi >> 20) & 0x01);
  int YearBCDLo     = ( (EvalValueMaskHi >> 21) & 0x0f);
  int YearBCDHi     = ( (EvalValueMaskHi >> 25) & 0x0f);

  int Day           = DayBCDLo    + 10 * DayBCDHi;
  int Month         = MonthBCDLo  + 10 * MonthBCDHi;
  int Century       = 20;
  int Year          = YearBCDLo   + 10 * YearBCDHi;
  int ParityDate    = ( (EvalValueMaskHi >>  7)      // 36 Start Day
                      ^ (EvalValueMaskHi >>  8)      // 37
                      ^ (EvalValueMaskHi >>  9)      // 38
                      ^ (EvalValueMaskHi >> 10)      // 39
                      ^ (EvalValueMaskHi >> 11)      // 40
                      ^ (EvalValueMaskHi >> 12)      // 41 End Day
                      ^ (EvalValueMaskHi >> 13)      // 42 Start Weekday
                      ^ (EvalValueMaskHi >> 14)      // 43
                      ^ (EvalValueMaskHi >> 15)      // 44 End Weekday
                      ^ (EvalValueMaskHi >> 16)      // 45 Start Month
                      ^ (EvalValueMaskHi >> 17)      // 46
                      ^ (EvalValueMaskHi >> 18)      // 47
                      ^ (EvalValueMaskHi >> 19)      // 48
                      ^ (EvalValueMaskHi >> 20)      // 49 End Month
                      ^ (EvalValueMaskHi >> 21)      // 50 Start Year
                      ^ (EvalValueMaskHi >> 22)      // 51
                      ^ (EvalValueMaskHi >> 23)      // 52
                      ^ (EvalValueMaskHi >> 24)      // 53
                      ^ (EvalValueMaskHi >> 25)      // 54
                      ^ (EvalValueMaskHi >> 26)      // 55
                      ^ (EvalValueMaskHi >> 27)      // 56
                      ^ (EvalValueMaskHi >> 28)      // 57 End Year
                      ^ (EvalValueMaskHi >> 29)      // 58 Parity Year
                      ) & 1;

  if ( (EvalValidMaskLo & 0x1FF70000) != 0x1FF70000
    || (EvalValidMaskHi & 0x3FFFFFFF) != 0x3FFFFFFF )
  {
    /* fprintf(errstream, "Error: Not enough bits collected\n"); */
  }
  else if ( (EvalValueMaskLo &   0x100000) != 0x100000 )
  {
    if (errstream)
      fprintf(errstream, "Error: Startbit 20 not set\n");
  }
  else if ( 0 == TimeZone || 3 == TimeZone )
  {
    if (errstream)
      fprintf(errstream, "Error: TimeZone neither MEZ nor MESZ\n");
  }
  else if ( 0 != ParityMinute )
  {
    if (errstream)
      fprintf(errstream, "Error: Defect Parity for Minute\n");
  }
  else if ( MinuteBCDLo < 0 || MinuteBCDLo > 9 )
  {
    if (errstream)
      fprintf(errstream, "Error: Lower digit for Minute not in range 0 .. 9\n");
  }
  else if ( MinuteBCDHi < 0 || MinuteBCDHi >= 6 )
  {
    if (errstream)
      fprintf(errstream, "Error: Higher digit for Minute not in range 0 .. 5\n");
  }
  else if ( 0 != ParityHour )
  {
    if (errstream)
      fprintf(errstream, "Error: Defect Parity for Hour\n");
  }
  else if ( HourBCDLo < 0 || HourBCDLo > 9 )
  {
    if (errstream)
      fprintf(errstream, "Error: Lower digit for Hour not in range 0 .. 9\n");
  }
  else if ( HourBCDHi < 0 || HourBCDHi >= 3 )
  {
    if (errstream)
      fprintf(errstream, "Error: Higher digit for Hour not in range 0 .. 2\n");
  }
  else if ( Hour >= 24 )
  {
    if (errstream)
      fprintf(errstream, "Error: Hour not in range 0 .. 23\n");
  }

  else if ( 0 != ParityDate )
  {
    if (errstream)
      fprintf(errstream, "Error: Defect Parity for Date\n");
  }
  else if ( DayBCDLo < 0 || DayBCDLo > 9 )
  {
    if (errstream)
      fprintf(errstream, "Error: Lower digit for Day not in range 0 .. 9\n");
  }
  else if ( DayBCDHi < 0 || DayBCDHi > 3 )
  {
    if (errstream)
      fprintf(errstream, "Error: Higher digit for Day not in range 0 .. 2\n");
  }
  else if ( Day < 1 || Day > 31 )
  {
    if (errstream)
      fprintf(errstream, "Error: Day not in range 1 .. 31\n");
  }
  else if ( Weekday < 1 || Weekday > 7 )
  {
    if (errstream)
      fprintf(errstream, "Error: Weekday not in range 1 .. 7\n");
  }
  else if ( MonthBCDLo < 0 || MonthBCDLo > 9 )
  {
    if (errstream)
      fprintf(errstream, "Error: Lower digit for Month not in range 0 .. 9\n");
  }
  else if ( MonthBCDHi < 0 || MonthBCDHi > 1 )
  {
    if (errstream)
      fprintf(errstream, "Error: Higher digit for Month not in range 0 .. 1\n");
  }
  else if ( Month < 1 || Month > 12 )
  {
    if (errstream)
      fprintf(errstream, "Error: Month not in range 1 .. 12\n");
  }
  else if ( YearBCDLo < 0 || YearBCDLo > 9 )
  {
    if (errstream)
      fprintf(errstream, "Error: Lower digit for Year not in range 0 .. 9\n");
  }
  else if ( YearBCDHi < 0 || YearBCDHi > 9 )
  {
    if (errstream)
      fprintf(errstream, "Error: Higher digit for Year not in range 0 .. 9\n");
  }
  else
  {
    tms->tm_sec   = 0; // 0 .. 59, 60 for leap sec
    tms->tm_min   = Minute; // 0 .. 59
    tms->tm_hour  = Hour; // 0 .. 23
    tms->tm_mday  = Day;  // 1 .. 31
    tms->tm_mon   = Month -1; // 0 .. 11
    tms->tm_year  = Century * 100 + Year - 1900; // years since 1900
    // tms->tm_wday  = 0; // mktime() ignores this
    tms->tm_wday  = (Weekday == 7) ? 0 : Weekday; // convert from 1..7 to 0..6
    tms->tm_yday  = 0; // mktime() ignores this
    tms->tm_isdst = (1 == TimeZone) ? 1 : 0; //positive if daylight saving time is in effect, zero if it is not, and negative if unknown
    *DCF_TZ_idx   = TimeZone;
    retval = true;
  }

  return retval;
}


void DCF77::newData( unsigned int framecount, const float * data )
{
  unsigned int i;
  unsigned int idx;
  bool  ReSync;
  float LocalLastSample;

  switch( eState )
  {
    case STATE_GET_THRESH:
      idx = ChanIdx;
      for ( i = 0; i < framecount; ++i, idx += ChanCount )
      {
        // gather statistics
        if ( data[idx] > Max )
          Max = data[idx];
        Sum += data[idx];
      }
      frameIndex += framecount;

      // evaluate statistics after 10 seconds
      if ( (double)frameIndex >= 10.0 * SampleRate )
      {
        const double dMean = Sum / frameIndex;
        Mean      = (float)dMean;
        // Signal Power should not exceed 7/10 th of Mean to Max voltage
        Threshold = (float)( dMean + 0.7 * ( Max - dMean ) );
        ThreshFinishMessage = true;
        // State finished --> next state := STATE_GET_TIME
        initGetTime();
      }
      break;

    case STATE_GET_TIME:

      LocalLastSample = LastSample;
      idx = ChanIdx;
      ReSync = false;

      if ( FramesSinceLastMinPulse >= 0 )
        FramesSinceLastMinPulse += framecount;

      for ( i = 0; i < framecount; ++i, idx += ChanCount )
      {
        if ( LocalLastSample < Threshold && data[idx] >= Threshold )
        {
          const float MSecsSinceLastPulse = (float)( ( FramesSinceLastPulse + i ) * 1000.0 / SampleRate );
          if      ( ( -1 == LastBit && MSecsSinceLastPulse >  60.0 && MSecsSinceLastPulse < 140.0 )  // ~ 100 ms
                  ||( -1 == LastBit && MSecsSinceLastPulse > 160.0 && MSecsSinceLastPulse < 240.0 )  // ~ 200 ms
                  )
          {
            LastBit = ( MSecsSinceLastPulse < 150.0 ) ? 0 : 1;
            // DCF bits 28 .. 0: add 1 new bit from Hi and shift old bits
            ValueMaskLo = ( (ValueMaskHi & 1) << 28 ) | ( ValueMaskLo >> 1 );
            ValidMaskLo = ( (ValidMaskHi & 1) << 28 ) | ( ValidMaskLo >> 1 );
            // DCF bits 58 .. 29 == 29 .. 0: add 1 new bit and shift old bits
            ValueMaskHi = ( LastBit << 29 ) | ( ValueMaskHi >> 1 );
            ValidMaskHi =       ( 1 << 29 ) | ( ValidMaskHi >> 1 );

            aiDiffFrames[iDiffIdx] = FramesSinceLastPulse + i;
            iDiffIdx = 1 - iDiffIdx;
            PrintDiff = 1;
            FramesSinceLastPulse = - (int)i;
          }
          else if ( ( 0 == LastBit && MSecsSinceLastPulse > 860.0 && MSecsSinceLastPulse < 940.0 )  // ~ 900 ms
                  ||( 1 == LastBit && MSecsSinceLastPulse > 760.0 && MSecsSinceLastPulse < 840.0 )  // ~ 800 ms
                  )
          {
            LastBit = -1;   // after 100 ms or 200 ms Pulse at Second pulse

            aiDiffFrames[iDiffIdx] = FramesSinceLastPulse + i;
            iDiffIdx = 1 - iDiffIdx;
            PrintDiff = 1;
            FramesSinceLastPulse = - (int)i;
          }
          else if ( ( 0 == LastBit && MSecsSinceLastPulse > 1860.0 && MSecsSinceLastPulse < 1940.0 )  // ~ 1900 ms
                  ||( 1 == LastBit && MSecsSinceLastPulse > 1760.0 && MSecsSinceLastPulse < 1840.0 )  // ~ 1800 ms
                  )
          {
            LastBit = -1; // after 100 ms or 200 ms Pulse at Minute pulse
            FramesSinceLastMinPulse = framecount - i;
            EvalValueMaskLo = ValueMaskLo;
            EvalValidMaskLo = ValidMaskLo;
            EvalValueMaskHi = ValueMaskHi;
            EvalValidMaskHi = ValidMaskHi;
            EvaluatedMinPulse = false;

            aiDiffFrames[iDiffIdx] = FramesSinceLastPulse + i;
            iDiffIdx = 1 - iDiffIdx;
            PrintDiff = 1;
            FramesSinceLastPulse = - (int)i;
          }
          else if ( MSecsSinceLastPulse >= 20000.0 && MSecsSinceLastPulse < 50000.0 )  // initial pulse search?
          {
            LastBit = -1;   // ignore
            FramesSinceLastPulse = - (int)i;
          }
          else if ( -1 == LastBit && MSecsSinceLastPulse < 30.0 )
          {
            // filter noise!
            LastBit = -1;
            //fprintf(stderr, "ignore after %f ms\n", MSecsSinceLastPulse);
            // do not set FramesSinceLastPulse !!!
          }
          else
          {
            //fprintf(stderr, "resync: with LastBit=%d after %f ms\n", LastBit, MSecsSinceLastPulse);
            ReSync = true;  // sync error!
            LastBit = -1;
            FramesSinceLastPulse = - (int)i;
          }
        }
        LocalLastSample = data[idx];
      } // end for
      FramesSinceLastPulse += framecount;
      LastSample = LocalLastSample;

      if ( ( FramesSinceLastPulse > 10.0 * SampleRate
          && FramesSinceLastPulse < 20.0 * SampleRate )
          || ReSync
          )
      {
        initGetThreshold();
      }
      break;

    default:
      ;
  }
}

