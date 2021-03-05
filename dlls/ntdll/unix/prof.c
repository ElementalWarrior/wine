/*
 * Profiling functions
 *
 * Copyright 2019 Rémi Bernon for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#if 0
#pragma makedep unix
#endif

#include "config.h"
#include "wine/port.h"

#include "wine/prof.h"
#include "wine/debug.h"
#include "ntdll_misc.h"

#include <assert.h>
#include <time.h>

WINE_DEFAULT_DEBUG_CHANNEL(prof);
WINE_DECLARE_DEBUG_CHANNEL(fprof);

static int prof_tid = -1;
static int trace_on = -1;
static int fprof_on = -1;

static size_t period_ticks;
static size_t spent_pctage;
static size_t ticks_per_ns;

static inline size_t prof_ticks(void)
{
    unsigned int hi, lo;
    __asm__ __volatile__ ("mfence; rdtscp; lfence" : "=&d" (hi), "=&a" (lo) :: "%rcx", "memory");
    return (size_t)(((unsigned long long)hi << 32) | (unsigned long long)lo);
}

static __cdecl __attribute__((noinline)) void prof_initialize(void)
{
    struct timespec time_start, time_end;
    size_t warmup_count = 5000000, time_ns, ticks, tick_overhead, rdtsc_overhead = ~0;

    clock_gettime(CLOCK_MONOTONIC, &time_start);
    ticks = -prof_ticks();

    while (--warmup_count)
    {
        tick_overhead = -prof_ticks();
        tick_overhead += prof_ticks();
        if (rdtsc_overhead > tick_overhead / 2) rdtsc_overhead = tick_overhead / 2;
    }

    clock_gettime(CLOCK_MONOTONIC, &time_end);
    ticks += prof_ticks();

    time_ns = (time_end.tv_sec - time_start.tv_sec) * 1000 * 1000 * 1000 + (time_end.tv_nsec - time_start.tv_nsec);
    ticks_per_ns = (ticks - rdtsc_overhead + time_ns / 2) / time_ns;
    if (ticks_per_ns == 0) ticks_per_ns = 1;

    period_ticks = (getenv("WINEPROF_PERIODNS") ? atoll(getenv("WINEPROF_PERIODNS")) : (1000000000ull / 30)) * ticks_per_ns;
    spent_pctage = getenv("WINEPROF_SPENTPCT") ? atoll(getenv("WINEPROF_SPENTPCT")) : 5;
    if (!getenv("WINEPROF_THREADID")) prof_tid = 0;
    else if (!strcmp(getenv("WINEPROF_THREADID"), "render")) prof_tid = 1;
    else prof_tid = atoll(getenv("WINEPROF_THREADID"));
}

struct __wine_prof_data *__cdecl __wine_prof_data_alloc(void)
{
    return malloc(sizeof(struct __wine_prof_data));
}

size_t __cdecl __wine_prof_start( struct __wine_prof_data *data )
{
    size_t start_ticks;

    if (!data || !data->name || !trace_on) return 0;
    if (__builtin_expect(trace_on == -1, 0) && !(trace_on = TRACE_ON(prof))) return 0;
    if (__builtin_expect(period_ticks == 0, 0)) prof_initialize();
    if (prof_tid != 0 && prof_tid != GetCurrentThreadId()) return 0;

    start_ticks = prof_ticks();
    if (__atomic_load_n(&data->print_ticks, __ATOMIC_ACQUIRE) == 0)
        __atomic_compare_exchange_n(&data->print_ticks, &data->print_ticks, start_ticks,
                                    0, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE);
    return start_ticks;
}

void __cdecl __wine_prof_stop( struct __wine_prof_data *data, size_t start_ticks )
{
    size_t stop_ticks, spent_ticks, total_ticks, accum_ticks, print_ticks, limit_ticks;

    if (!data || !data->name || !trace_on) return;
    if (prof_tid != 0 && prof_tid != GetCurrentThreadId()) return;

    stop_ticks = prof_ticks();
    print_ticks = data->print_ticks;
    spent_ticks = stop_ticks - start_ticks;
    total_ticks = stop_ticks - print_ticks;
    accum_ticks = __atomic_add_fetch(&data->accum_ticks, spent_ticks, __ATOMIC_ACQUIRE);

    limit_ticks = period_ticks;
    if (data->limit_ns) limit_ticks = data->limit_ns * ticks_per_ns;

    if (spent_ticks * 100 >= limit_ticks)
    {
        TRACE("%s: time spike: %zu (ns) %zu (%%)\n", data->name,
            spent_ticks / ticks_per_ns, spent_ticks * 100u / limit_ticks);
    }

    if (total_ticks >= limit_ticks &&
        __atomic_compare_exchange_n(&data->print_ticks, &print_ticks, stop_ticks,
                                    0, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE))
    {
        if (accum_ticks * 100 >= spent_pctage * total_ticks)
            TRACE("%s: time spent: %zu (ns) %zu (%%)\n", data->name,
                accum_ticks / ticks_per_ns, accum_ticks * 100u / total_ticks);
        __atomic_fetch_sub(&data->accum_ticks, accum_ticks, __ATOMIC_RELEASE);
    }
}

void __cdecl __wine_prof_frame( struct __wine_prof_frame_data *data )
{
    size_t time_ticks, i;

    if (prof_tid == 1) prof_tid = GetCurrentThreadId();
    if (!data || !data->name || !fprof_on) return;
    if (__builtin_expect(fprof_on == -1, 0) && !(fprof_on = TRACE_ON(fprof))) return;
    if (__builtin_expect(period_ticks == 0, 0)) prof_initialize();

    time_ticks = prof_ticks();

    if (!data->prev_ticks)
    {
        data->prev_ticks = time_ticks;
        data->print_ticks = time_ticks;
        return;
    }

    data->time_ticks[data->time_count++] = time_ticks - data->prev_ticks;
    data->prev_ticks = time_ticks;

    if ((time_ticks - data->print_ticks) / 1000 / 1000 < 5000 &&
        data->time_count < 1024)
        return;

    for (i = 0; i < data->time_count; ++i)
    {
        TRACE_(fprof)("%s: %zu %zu\n", data->name, data->print_ticks / ticks_per_ns, data->time_ticks[i] / ticks_per_ns);
        data->print_ticks += data->time_ticks[i];
    }

    data->time_count = 0;
    data->print_ticks = time_ticks;
}
