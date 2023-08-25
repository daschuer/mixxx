#include "highresolution_monotonic_clock.h"

#include <chrono>

// namespace mixxx {

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

#include <QtCore/qsystemdetection.h>

#if defined(Q_OS_MAC)
#include <mach/mach_time.h>
#include <sys/time.h>
#include <unistd.h>
#elif defined(Q_OS_UNIX)
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <windows.h>
#endif

// mac/unix code heavily copied from QElapsedTimer

#if false

static mach_timebase_info_data_t info = {0, 0};
static std::chrono::nanoseconds absoluteToNSecs(qint64 cpuTime) {
    if (info.denom == 0)
        mach_timebase_info(&info);
    return std::chrono::nanoseconds(cpuTime * info.numer / info.denom);
}

auto HighResolutionMonotonicClockFallback::now() noexcept -> time_point {
    return time_point(absoluteToNSecs(mach_absolute_time()));
}

#elif defined(Q_OS_UNIX) || defined(Q_OS_MAC)

// #if (_POSIX_MONOTONIC_CLOCK - 0 != 0)
// static std::atomic<bool> monotonicClockChecked = true;
// static std::atomic<bool> monotonicClockAvailable = _POSIX_MONOTONIC_CLOCK > 0;
// #else
// static std::atomic<bool> monotonicClockChecked = false;
// static std::atomic<bool> monotonicClockAvailable = false;
// #endif

// static void unixCheckClockType() {
// #if (_POSIX_MONOTONIC_CLOCK - 0 == 0)
//     if (monotonicClockChecked.load(std::memory_order_acquire)) [[likely]] {
//         return;
//     }

// #if defined(_SC_MONOTONIC_CLOCK)
//     // detect if the system support monotonic timers
//     long x = sysconf(_SC_MONOTONIC_CLOCK);
//     monotonicClockAvailable.store(x >= 200112L, std::memory_order_release);
// #endif
//     monotonicClockAvailable.store(true, std::memory_order_release);
// #endif
// }

auto HighResolutionMonotonicClockFallback::now() noexcept -> time_point {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return time_point(std::chrono::seconds(ts.tv_sec) + std::chrono::nanoseconds(ts.tv_nsec));
}

#elif defined(Q_OS_WIN)

static qint64 getTimeFromTick(qint64 elapsed) {
    static LARGE_INTEGER freq = {{0, 0}};
    if (!freq.QuadPart)
        QueryPerformanceFrequency(&freq);
    return 1000000000 * elapsed / freq.QuadPart;
}

auto HighResolutionMonotonicClockFallback::now() noexcept -> time_point {
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return time_point(std::chrono::nanoseconds(getTimeFromTick(li.QuadPart)));
}

#else

#error Your Platform does not have a high resolution monotonic clock implementation out-of-the-box. Please implement HighResolutionMonotonicClockFallback::now() for your platform.

#endif

// } // namespace mixxx
