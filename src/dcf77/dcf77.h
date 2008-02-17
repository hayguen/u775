
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

#ifndef _U775_DCF77_H_
#define _U775_DCF77_H_

#ifndef _SVID_SOURCE
#define _SVID_SOURCE /* glibc2 needs this */
#endif
#include <time.h>
#include <stdio.h>

class DCF77
{
public:
  DCF77();
  ~DCF77();

  void initGetThreshold();
  void initGetTime();
  void newData( unsigned int framecount, const float * data );
  bool evalMinPulse(struct tm * tms, int * DCF_TZ_idx, FILE * errstream);

  typedef enum
  {
      STATE_GET_THRESH  /// initialer Zustand
    , STATE_GET_TIME
  }
    State;

  double          SampleRate;
  State            eState;
  unsigned        ChanCount;
  unsigned        ChanIdx;
  unsigned        FramesPerBuffer;
  unsigned        frameIndex;  /* Index into sample array. */

  // vars for state STATE_GET_THRESH
  double          Sum;
  volatile float Max;
  // result of state STATE_GET_THRESH
  volatile bool  ThreshStartMessage;
  volatile bool  ThreshFinishMessage;
  volatile float Mean;
  volatile float Threshold;

  // vars for state STATE_GET_TIME
  float           LastSample;
  int             FramesSinceLastPulse;
  int             LastBit;
  int             ValueMaskLo;  // DCF bits 28 .. 0
  int             ValidMaskLo;
  int             ValueMaskHi;  // DCF bits 58 .. 29
  int             ValidMaskHi;
  // result of state STATE_GET_TIME
  volatile int   FramesSinceLastMinPulse;
  volatile bool  EvaluatedMinPulse;
  volatile int   EvalValueMaskLo;  // DCF bits 28 .. 0
  volatile int   EvalValidMaskLo;
  volatile int   EvalValueMaskHi;  // DCF bits 58 .. 29
  volatile int   EvalValidMaskHi;

  volatile int   aiDiffFrames[2];
  volatile int   iDiffIdx;
  volatile int   PrintDiff;

  int            SetSysTime;
};

#endif /* _U775_DCF77_H_ */

