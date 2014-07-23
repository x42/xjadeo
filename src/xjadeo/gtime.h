#ifndef XJ_GTIME_H
#define XJ_GTIME_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef PLATFORM_WINDOWS
#if (!defined __uint32_t_defined && !defined _STDINT_H)
typedef long long int int64_t;
typedef long long unsigned int uint64_t;
typedef int uint32_t;
#endif
#ifndef INT_MAX
#define INT_MAX 2147483647
#endif
#else
#include <stdint.h>
#endif

/**
 * xj_get_monotonic_time:
 *
 * Queries the system monotonic time.
 *
 * The monotonic clock will always increase and doesn't suffer
 * discontinuities when the user (or NTP) changes the system time.  It
 * may or may not continue to tick during times where the machine is
 * suspended.
 *
 * We try to use the clock that corresponds as closely as possible to
 * the passage of time as measured by system calls such as poll() but it
 * may not always be possible to do this.
 *
 * Returns: the monotonic time, in microseconds
 **/

int64_t xj_get_monotonic_time (void);

#endif // XJ_GTIME_H
