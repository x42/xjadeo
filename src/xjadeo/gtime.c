/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gmain.c: Main loop abstraction, timeouts, and idle functions
 * Copyright 1998 Owen Taylor
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "gtime.h"

#ifdef PLATFORM_WINDOWS

#include <windows.h>

static ULONGLONG (*g_GetTickCount64) (void) = NULL;
static uint32_t g_win32_tick_epoch = 0;
static int clock_initialized = 0;

static void
xj_clock_win32_init (void)
{
	HMODULE kernel32;

	g_GetTickCount64 = NULL;
	kernel32 = GetModuleHandle ("KERNEL32.DLL");
	if (kernel32 != NULL)
		g_GetTickCount64 = (void *) GetProcAddress (kernel32, "GetTickCount64");
	g_win32_tick_epoch = ((uint32_t)GetTickCount()) >> 31;
}

int64_t xj_get_monotonic_time (void) {
	if (!clock_initialized) {
		xj_clock_win32_init();
		clock_initialized = 1;
	}
  uint64_t ticks;
  uint32_t ticks32;

  /* There are four sources for the monotonic time on Windows:
   *
   * Three are based on a (1 msec accuracy, but only read periodically) clock chip:
   * - GetTickCount (GTC)
   *    32bit msec counter, updated each ~15msec, wraps in ~50 days
   * - GetTickCount64 (GTC64)
   *    Same as GetTickCount, but extended to 64bit, so no wrap
   *    Only available in Vista or later
   * - timeGetTime (TGT)
   *    similar to GetTickCount by default: 15msec, 50 day wrap.
   *    available in winmm.dll (thus known as the multimedia timers)
   *    However apps can raise the system timer clock frequency using timeBeginPeriod()
   *    increasing the accuracy up to 1 msec, at a cost in general system performance
   *    and battery use.
   *
   * One is based on high precision clocks:
   * - QueryPrecisionCounter (QPC)
   *    This has much higher accuracy, but is not guaranteed monotonic, and
   *    has lots of complications like clock jumps and different times on different
   *    CPUs. It also has lower long term accuracy (i.e. it will drift compared to
   *    the low precision clocks.
   *
   * Additionally, the precision available in the timer-based wakeup such as
   * MsgWaitForMultipleObjectsEx (which is what the mainloop is based on) is based
   * on the TGT resolution, so by default it is ~15msec, but can be increased by apps.
   *
   * The QPC timer has too many issues to be used as is. The only way it could be used
   * is to use it to interpolate the lower precision clocks. Firefox does something like
   * this:
   *   https://bugzilla.mozilla.org/show_bug.cgi?id=363258
   *
   * However this seems quite complicated, so we're not doing this right now.
   *
   * The approach we take instead is to use the TGT timer, extending it to 64bit
   * either by using the GTC64 value, or if that is not available, a process local
   * time epoch that we increment when we detect a timer wrap (assumes that we read
   * the time at least once every 50 days).
   *
   * This means that:
   *  - We have a globally consistent monotonic clock on Vista and later
   *  - We have a locally monotonic clock on XP
   *  - Apps that need higher precision in timeouts and clock reads can call
   *    timeBeginPeriod() to increase it as much as they want
   */

  if (g_GetTickCount64 != NULL)
    {
      uint32_t ticks_as_32bit;

      ticks = g_GetTickCount64 ();
      ticks32 = timeGetTime();

      /* GTC64 and TGT are sampled at different times, however they
       * have the same base and source (msecs since system boot).
       * They can differ by as much as -16 to +16 msecs.
       * We can't just inject the low bits into the 64bit counter
       * as one of the counters can have wrapped in 32bit space and
       * the other not. Instead we calculate the signed difference
       * in 32bit space and apply that difference to the 64bit counter.
       */
      ticks_as_32bit = (uint32_t)ticks;

      /* We could do some 2's complement hack, but we play it safe */
      if (ticks32 - ticks_as_32bit <= INT_MAX)
        ticks += ticks32 - ticks_as_32bit;
      else
        ticks -= ticks_as_32bit - ticks32;
    }
  else
    {
      uint32_t epoch = g_win32_tick_epoch;

      /* Must read ticks after the epoch. Then we're guaranteed
       * that the ticks value we read is higher or equal to any
       * previous ones that lead to the writing of the epoch.
       */
      ticks32 = timeGetTime();

      /* We store the MSB of the current time as the LSB
       * of the epoch. Comparing these bits lets us detect when
       * the 32bit counter has wrapped so we can increase the
       * epoch.
       *
       * This will work as long as this function is called at
       * least once every ~24 days, which is half the wrap time
       * of a 32bit msec counter. I think this is pretty likely.
       *
       * Note that g_win32_tick_epoch is a process local state,
       * so the monotonic clock will not be the same between
       * processes.
       */
      if ((ticks32 >> 31) != (epoch & 1))
        {
          epoch++;
          g_win32_tick_epoch = epoch;
        }


      ticks = (uint64_t)ticks32 | ((uint64_t)epoch) << 31;
    }

  return ticks * 1000;
}

#elif defined PLATFORM_OSX

#include <mach/mach.h>
#include <mach/mach_time.h>
#include <stdio.h>

int64_t xj_get_monotonic_time (void) {
  static mach_timebase_info_data_t timebase_info;

  if (timebase_info.denom == 0)
    {
      /* This is a fraction that we must use to scale
       * mach_absolute_time() by in order to reach nanoseconds.
       *
       * We've only ever observed this to be 1/1, but maybe it could be
       * 1000/1 if mach time is microseconds already, or 1/1000 if
       * picoseconds.  Try to deal nicely with that.
       */
      mach_timebase_info (&timebase_info);

      /* We actually want microseconds... */
      if (timebase_info.numer % 1000 == 0)
        timebase_info.numer /= 1000;
      else
        timebase_info.denom *= 1000;

      /* We want to make the numer 1 to avoid having to multiply... */
      if (timebase_info.denom % timebase_info.numer == 0)
        {
          timebase_info.denom /= timebase_info.numer;
          timebase_info.numer = 1;
        }
      else
        {
          /* We could just multiply by timebase_info.numer below, but why
           * bother for a case that may never actually exist...
           *
           * Plus -- performing the multiplication would risk integer
           * overflow.  If we ever actually end up in this situation, we
           * should more carefully evaluate the correct course of action.
           */
          mach_timebase_info (&timebase_info); /* Get a fresh copy for a better message */
          fprintf(stderr, "Got weird mach timebase info of %d/%d.\n",
                   timebase_info.numer, timebase_info.denom);
        }
    }

  return mach_absolute_time () / timebase_info.denom;
}

#else

#include <time.h>

int64_t xj_get_monotonic_time (void) {
  struct timespec ts;
  clock_gettime (CLOCK_MONOTONIC, &ts);
  return (((int64_t) ts.tv_sec) * 1000000) + (ts.tv_nsec / 1000);
}
#endif
