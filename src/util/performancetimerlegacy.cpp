/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "performancetimerlegacy.h"

#if defined(Q_OS_MAC)
#include <mach/mach_time.h>
#include <sys/time.h>
#include <unistd.h>
#elif defined(Q_OS_SYMBIAN)
#include <e32std.h>
#include <hal.h>
#include <hal_data.h>
#include <sys/time.h>
#elif defined(Q_OS_UNIX)
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <windows.h>
#endif

// mac/unix code heavily copied from QElapsedTimer

////////////////////////////// Mac //////////////////////////////
#if defined(Q_OS_MAC)

static mach_timebase_info_data_t info = {0, 0};
static qint64 absoluteToNSecs(qint64 cpuTime) {
    if (info.denom == 0)
        mach_timebase_info(&info);
    qint64 nsecs = cpuTime * info.numer / info.denom;
    return nsecs;
}

void PerformanceTimerLegacy::start() {
    t1 = mach_absolute_time();
}

mixxx::Duration PerformanceTimerLegacy::elapsed() const {
    uint64_t cpu_time = mach_absolute_time();
    return mixxx::Duration::fromNanos(absoluteToNSecs(cpu_time - t1));
}

mixxx::Duration PerformanceTimerLegacy::restart() {
    qint64 start;
    start = t1;
    t1 = mach_absolute_time();
    return mixxx::Duration::fromNanos(absoluteToNSecs(t1 - start));
}

mixxx::Duration PerformanceTimerLegacy::difference(const PerformanceTimerLegacy& timer) const {
    return mixxx::Duration::fromNanos(absoluteToNSecs(t1 - timer.t1));
}

////////////////////////////// Symbian //////////////////////////////
#elif defined(Q_OS_SYMBIAN)

static qint64 getTimeFromTick(qint64 elapsed) {
    static TInt freq = 0;
    if (!freq)
        HAL::Get(HALData::EFastCounterFrequency, freq);

    return (elapsed * 1000000000) / freq;
}

void PerformanceTimerLegacy::start() {
    t1 = User::FastCounter();
}

mixxx::Duration PerformanceTimerLegacy::elapsed() const {
    return mixxx::Duration::fromNanos(getTimeFromTick(User::FastCounter() - t1));
}

mixxx::Duration PerformanceTimerLegacy::restart() {
    qint64 start;
    start = t1;
    t1 = User::FastCounter();
    return mixxx::Duration::fromNanos(getTimeFromTick(t1 - start));
}

mixxx::Duration PerformanceTimerLegacy::difference(const PerformanceTimerLegacy& timer) const {
    return mixxx::Duration::fromNanos(getTimeFromTick(t1 - timer.t1));
}

////////////////////////////// Unix //////////////////////////////
#elif defined(Q_OS_UNIX)

// #if defined(QT_NO_CLOCK_MONOTONIC) || defined(QT_BOOTSTRAPPED)
// // turn off the monotonic clock
// #ifdef _POSIX_MONOTONIC_CLOCK
// #undef _POSIX_MONOTONIC_CLOCK
// #endif
// #define _POSIX_MONOTONIC_CLOCK -1
// #endif

// #if (_POSIX_MONOTONIC_CLOCK - 0 != 0)
// static const bool monotonicClockChecked = true;
// static const bool monotonicClockAvailable = _POSIX_MONOTONIC_CLOCK > 0;
// #else
// static int monotonicClockChecked = false;
// static int monotonicClockAvailable = false;
// #endif

// #ifdef Q_CC_GNU
// #define is_likely(x) __builtin_expect((x), 1)
// #else
// #define is_likely(x) (x)
// #endif
// #define load_acquire(x) ((volatile const int&)(x))
// #define store_release(x, v) ((volatile int&)(x) = (v))

// static void unixCheckClockType() {
// #if (_POSIX_MONOTONIC_CLOCK - 0 == 0)
//     if (is_likely(load_acquire(monotonicClockChecked))) {
//         return;
//     }

// #if defined(_SC_MONOTONIC_CLOCK)
//     // detect if the system support monotonic timers
//     long x = sysconf(_SC_MONOTONIC_CLOCK);
//     store_release(monotonicClockAvailable, x >= 200112L);
// #endif

//     store_release(monotonicClockChecked, true);
// #endif
// }

void PerformanceTimerLegacy::start() {
    timespec ts;
    // just assume CLOCK_MONOTONIC is available
    clock_gettime(CLOCK_MONOTONIC, &ts);
    t1 = ts.tv_sec;
    t2 = ts.tv_nsec;
}

mixxx::Duration PerformanceTimerLegacy::elapsed() const {
    timespec ts;
    // just assume CLOCK_MONOTONIC is available
    clock_gettime(CLOCK_MONOTONIC, &ts);
    qint64 sec = ts.tv_sec - t1;
    qint64 frac = ts.tv_nsec - t2;

    return mixxx::Duration::fromNanos(sec * Q_INT64_C(1000000000) + frac);
}

mixxx::Duration PerformanceTimerLegacy::restart() {
    timespec ts;
    // just assume CLOCK_MONOTONIC is available
    clock_gettime(CLOCK_MONOTONIC, &ts);
    qint64 sec = ts.tv_sec - t1;
    qint64 frac = ts.tv_nsec - t2;
    t1 = ts.tv_sec;
    t2 = ts.tv_nsec;
    return mixxx::Duration::fromNanos(sec * Q_INT64_C(1000000000) + frac);
}

mixxx::Duration PerformanceTimerLegacy::difference(const PerformanceTimerLegacy& timer) const {
    qint64 sec, frac;
    sec = t1 - timer.t1;
    frac = t2 - timer.t2;
    return mixxx::Duration::fromNanos(sec * Q_INT64_C(1000000000) + frac);
}

////////////////////////////// Windows //////////////////////////////
#elif defined(Q_OS_WIN)

static qint64 getTimeFromTick(qint64 elapsed) {
    static LARGE_INTEGER freq = {{0, 0}};
    if (!freq.QuadPart)
        QueryPerformanceFrequency(&freq);
    return 1000000000 * elapsed / freq.QuadPart;
}

void PerformanceTimerLegacy::start() {
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    t1 = li.QuadPart;
}

mixxx::Duration PerformanceTimerLegacy::elapsed() const {
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return mixxx::Duration::fromNanos(getTimeFromTick(li.QuadPart - t1));
}

mixxx::Duration PerformanceTimerLegacy::restart() {
    LARGE_INTEGER li;
    qint64 start;
    start = t1;
    QueryPerformanceCounter(&li);
    t1 = li.QuadPart;
    return mixxx::Duration::fromNanos(getTimeFromTick(t1 - start));
}

mixxx::Duration PerformanceTimerLegacy::difference(const PerformanceTimerLegacy& timer) const {
    return mixxx::Duration::fromNanos(getTimeFromTick(t1 - timer.t1));
}

////////////////////////////// Default //////////////////////////////
#else

// default implementation (no hi-perf timer) does nothing
void PerformanceTimerLegacy::start() {
}

mixxx::Duration PerformanceTimerLegacy::elapsed() const {
    return mixxx::Duration::fromNanos(0);
}

mixxx::Duration PerformanceTimerLegacy::restart() const {
    return mixxx::Duration::fromNanos(0);
}

mixxx::Duration PerformanceTimerLegacy::difference(const PerformanceTimerLegacy& timer) const {
    return mixxx::Duration::fromNanos(0);
}

#endif
