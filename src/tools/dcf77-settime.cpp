
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


#ifdef _MSC_VER
#include <windows.h>
#endif

#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <portaudio.h>

#include "../dcf77/dcf77.h"


// FramesPerBuffer (=10ms) should be the accuracy of the clock
// Evaluation for the Minute Pulse in main() can be done slower (100ms),
// as the time gap in between is accounted by FramesSinceLastMinPulse
// TODO: accounting for default latencies:
//   1- signal latency from DCF transmitter (Mainflingen near Frankfurt
//      in Germany) to place of receiption. This can be calculated using
//      the distance
//   2- latency from microphone input to processing in this application
//      This may be estimated with a direct connection from sound
//      line-out to mic-in and having a test program!
//   3- latency from starting a 3rd application / setting system time
//      to being executed.
//

// a bit annoying having to discriminate the API version
// why do they change the API?
// however: thanks for providing a usable platform independent Sound Lib
#define VER_18_1  ( ( 18 << 16 ) | 1 )
#define VER_19    ( ( 19 << 16 ) | 0 )

#ifdef _MSC_VER
  // i've installed/compiled pa_stable_v19_20071207.tar.gz
  // on MS Windows with MS Visual C++ 2003.NET
  // pa_stable_v19_20071207.tar.gz contains PortAudio Version 19.0
  #define PORTAUDIO_LIB_VERSION  VER_19
#else
  // Ubuntu Linux 6.06.1 LTS comes with PortAudio 18.1
  #define PORTAUDIO_LIB_VERSION  VER_18_1
#endif


static void
pa_error_handler (PaError pa_error)
{
  printf("PortAudio error: %s\n", Pa_GetErrorText(pa_error));

  Pa_Terminate();
  exit (1);
}


/* This routine will be called by the PortAudio engine when audio is needed.
** It may be called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/

#if ( PORTAUDIO_LIB_VERSION >= VER_19 )

static int recordCallback(
    const void *inputBuffer
  , void *outputBuffer
  , unsigned long framesPerBuffer
  , const PaStreamCallbackTimeInfo* timeInfo
  , PaStreamCallbackFlags statusFlags
  , void *userData
  )

#else

static int recordCallback(
    void *inputBuffer
  , void *outputBuffer
  , unsigned long framesPerBuffer
  , PaTimestamp timeInfo
  , void *userData
  )

#endif
{
  DCF77 *data = (DCF77*)userData;

  data->newData( framesPerBuffer, (const float *)inputBuffer );
  return 0; // Continue
}


int main( int argc, char *argv[] )
{
  int argno;
  int ListDevices = 0;
  int DeviceCount = 0;
  int DeviceNo = 0;
  PaError pa_error;
  DCF77 data;
  double sumJitter = 0.0;
  double cntJitter = 0.0;
  const char * TZStrTab[] =
  {   "Err"
    , "MESZ (UTC+2)"
    , "MEZ (UTC+1)"
    , "Err"
  };
  const char * WeekDayStrTab[] =
  {   "Sunday"
    , "Monday"
    , "Tuesday"
    , "Wednesday"
    , "Thursday"
    , "Friday"
    , "Saturday"
  };

  printf("Pa_Initialize()\n");
  pa_error = Pa_Initialize();
  if (pa_error != paNoError)
    pa_error_handler (pa_error);
  printf("Pa_Initialize() done\n\n");


#if ( PORTAUDIO_LIB_VERSION >= VER_19 )
  DeviceCount = Pa_GetDeviceCount();
#else
  DeviceCount = Pa_CountDevices();
#endif

  if (DeviceCount < 0 )
    pa_error_handler(DeviceCount);

  if (DeviceCount == 0)
  {
    fprintf (stderr, "No PortAudio devices found\n");
    exit(0);
  }

#if ( PORTAUDIO_LIB_VERSION >= VER_19 )
  DeviceNo = Pa_GetDefaultInputDevice(); /* default input device */;
#else
  DeviceNo = Pa_GetDefaultInputDeviceID();
#endif

  printf("Default Input Device = %d\n", DeviceNo);

  printf("\nParsing Command Lines Arguments\n");
  for ( argno = 1; argno < argc; ++argno )
  {
    if ( !strcmp(argv[argno], "--help") )
    {
      printf("%s [--help] [--list] [left|right] [44100|48000] [setsystime] [<deviceno>]\n\n", argv[0]);
    }
    else if ( !strcmp(argv[argno], "--list") )
      ListDevices = 1;
    else if ( !strcmp(argv[argno], "left") )
    {
      data.ChanIdx = 0;
      printf("Channl := Left (=0)\n");
    }
    else if ( !strcmp(argv[argno], "right") )
    {
      data.ChanIdx = 1;
      printf("Channl := Right (=1)\n");
    }
    else if ( !strcmp(argv[argno], "48000") )
    {
      data.SampleRate = atof(argv[argno]);
      data.FramesPerBuffer = 480; // = 10 * 48000 / 1000 == 10 ms
      printf("SampleRate := %f\n", data.SampleRate);
    }
    else if ( !strcmp(argv[argno], "44100") )
    {
      data.SampleRate = atof(argv[argno]);
      data.FramesPerBuffer = 441; // = 10 * 44100 / 1000 == 10 ms
      printf("SampleRate := %f\n", data.SampleRate);
    }
    else if ( !strcmp(argv[argno], "setsystime") )
    {
      data.SetSysTime = 1;
    }
    else
    {
      int tmp = atoi(argv[argno]);
      if ( tmp >= 0 && tmp < DeviceCount )
      {
        DeviceNo = tmp;
      }
      else
        fprintf(stderr, "ignoring argument '%s'! DeviceNo not in Range 0 .. %d\n", argv[argno], DeviceCount-1 );
    }
  }
  printf("End of Parsing Command Lines Arguments\n\n");

  if ( ListDevices )
  {
    printf("\n\nListing Devices\n");
    int i;
    for ( i = 0; i < DeviceCount; ++i )
    {
      const PaDeviceInfo *device_info = Pa_GetDeviceInfo(i);

      printf("%d: %s\n", i, device_info->name);
      printf("  maxInputChannels = %d\n", device_info->maxInputChannels);
#if ( PORTAUDIO_LIB_VERSION >= VER_19 )
      printf("  default SampleRate = %f\n", device_info->defaultSampleRate);
#endif

      if (device_info->maxInputChannels < 1)
        continue;

#if ( PORTAUDIO_LIB_VERSION >= VER_19 )

      PaStreamParameters input_param;

      memset (&input_param, sizeof(input_param), 0);

      input_param.device = i;
      input_param.channelCount = data.ChanCount = device_info->maxInputChannels;
      input_param.sampleFormat = paFloat32;
      input_param.hostApiSpecificStreamInfo = NULL;

      pa_error = Pa_IsFormatSupported(&input_param, NULL, data.SampleRate);
      if (pa_error == paFormatIsSupported)
        printf ("  supports SampleRate and Format\n");
      else
      {
        printf ("  does NOT support SampleRate and Format\n");
        printf ("  PortAudio error: %s\n", Pa_GetErrorText(pa_error));
      }

#endif


    }
    printf("End of Listing Devices\n\n\n");
  }

  do
  {
    int i = DeviceNo;
    const PaDeviceInfo *device_info = Pa_GetDeviceInfo(i);

    printf("%d: %s\n", i, device_info->name);
    printf("  maxInputChannels = %d\n", device_info->maxInputChannels);
#if ( PORTAUDIO_LIB_VERSION >= VER_19 )
    printf("  default SampleRate = %f\n", device_info->defaultSampleRate);
#endif

    if (device_info->maxInputChannels < 1)
      break;

#if ( PORTAUDIO_LIB_VERSION >= VER_19 )
    PaStreamParameters input_param;
    memset (&input_param, sizeof(input_param), 0);

    input_param.device = i;
    input_param.channelCount = device_info->maxInputChannels;
    input_param.sampleFormat = paFloat32;
    input_param.hostApiSpecificStreamInfo = NULL;

    pa_error = Pa_IsFormatSupported(&input_param, NULL, data.SampleRate);
    if (pa_error == paFormatIsSupported)
      printf ("  supports SampleRate and Format\n");
    else
    {
      printf ("  does NOT support SampleRate and Format\n");
      printf ("  PortAudio error: %s\n", Pa_GetErrorText(pa_error));
    }
#endif
  } while (0);

  {
    PaStream*           stream;
    PaError             err = paNoError;

    data.frameIndex = 0;

#if ( PORTAUDIO_LIB_VERSION >= VER_19 )
    PaStreamParameters  inputParameters;
    inputParameters.device = DeviceNo;
    inputParameters.channelCount = data.ChanCount;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    /* Record some audio. -------------------------------------------- */
    err = Pa_OpenStream(
              &stream
            , &inputParameters
            , NULL                  /* &outputParameters, */
            , data.SampleRate
            , data.FramesPerBuffer
            , paClipOff      /* we won't output out of range samples so don't bother clipping them */
            , recordCallback
            , &data
            );
#else
    err = Pa_OpenStream(
              &stream
            , DeviceNo   // inputDevice
            , data.ChanCount     // numInputChannels
            , paFloat32  // inputSampleFormat
            , NULL       // void *inputDriverInfo
            , paNoDevice // outputDevice
            , 0          // numOutputChannels
            , 0          // outputSampleFormat
            , NULL       // void *outputDriverInfo
            , data.SampleRate // sampleRate
            , data.FramesPerBuffer // framesPerBuffer
            , (int)ceil(data.SampleRate * 0.100 / (double)data.FramesPerBuffer) // numberOfBuffers
            , paNoFlag   // streamFlags
            , recordCallback // PortAudioCallback *callback
            , &data      // void *userData
            );
#endif

    if( err != paNoError ) goto done;

    err = Pa_StartStream( stream );
    if( err != paNoError ) goto done;
    printf("\n\nNow recording!!\n"); fflush(stdout);

#if ( PORTAUDIO_LIB_VERSION >= VER_19 )
    while( 1 == ( err = Pa_IsStreamActive( stream ) ) )
#else
    while( 1 == ( err = Pa_StreamActive( stream ) ) )
#endif
    {
      Pa_Sleep(100);

      if ( data.ThreshStartMessage )
      {
        fprintf(stdout, "\nState STATE_GET_THRESH\n");
        fflush(stdout);
        data.ThreshStartMessage = false;
      }

      if ( data.ThreshFinishMessage )
      {
        fprintf(stdout, " => Mean = %.3f\n", data.Mean );
        fprintf(stdout, " => Threshold = %.3f\n", data.Threshold);
        fprintf(stdout, " => Max  = %.3f\n\n", data.Max );
        fflush(stdout);
        data.ThreshFinishMessage = false;
      }

      if ( data.PrintDiff )
      {
        data.PrintDiff = 0;
        if ( data.aiDiffFrames[1-data.iDiffIdx] > data.aiDiffFrames[data.iDiffIdx] )
        {
          double jitter = data.SampleRate - data.aiDiffFrames[0] - data.aiDiffFrames[1];
          if ( fabs(jitter) < 0.5*data.SampleRate )
          {
            sumJitter += jitter;
            cntJitter += 1.0;
#if 0
            double meanJitter = sumJitter / cntJitter;
            fprintf(stdout, "Pulse: %.1f %.1f\n", jitter, meanJitter );
#endif
          }
        }
      }
      if ( !data.EvaluatedMinPulse )
      {
        struct tm tms;
        int DCF_TZ_idx;
        // int Year, Month, Day, Weekday, Hour, Minute;
        if (data.evalMinPulse(&tms,&DCF_TZ_idx,stderr))
        {
          if (data.SetSysTime)
          {
            time_t tim;
            tim = mktime(&tms);
#ifdef _MSC_VER
            tms = * gmtime(&tim); // convert to UTC
            // and set to Windows system time structure
            SYSTEMTIME systime;
            systime.wYear = tms.tm_year + 1900;
            systime.wMonth = tms.tm_mon +1; // 1 .. 12
            systime.wDayOfWeek = tms.tm_wday;
            systime.wDay = tms.tm_mday;
            systime.wHour = tms.tm_hour;
            systime.wMinute = tms.tm_min;
            systime.wSecond = tms.tm_sec;
            systime.wMilliseconds = 0;
            if ( 0 == SetSystemTime(&systime) )
              fprintf(stderr, "Error setting system time\n");
#else
            if ( -1 == stime(&tim))
              fprintf(stderr, "Error setting system time: '%s'\n", strerror(errno) );
#endif
          }
          fprintf(stdout, "Date: %s, %04d-%02d-%02d  Time: %02d:%02d  %s  %f ms\n"
                        , WeekDayStrTab[tms.tm_wday], tms.tm_year + 1900, tms.tm_mon +1, tms.tm_mday
                        , tms.tm_hour, tms.tm_min
                        , TZStrTab[DCF_TZ_idx]
                        , (1000.0 * data.FramesSinceLastMinPulse / data.SampleRate)
                        );
          if (data.SetSysTime)
            goto done;
        }
        data.EvaluatedMinPulse = true;
        fflush(stdout);
      } // end if ( !data.EvaluatedMinPulse )
    } // end while( 1 == ( err = Pa_StreamActive( stream ) ) )

    if( err < 0 )
      goto done;

    err = Pa_CloseStream( stream );
    if( err != paNoError )
      goto done;

  }

done:

  printf("\n\nPa_Terminate()\n");
  Pa_Terminate();

  return 0;
}
