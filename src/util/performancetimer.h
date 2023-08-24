#pragma once
#include <chrono>

#include "util/duration.h"
#include "util/highresolution_monotonic_clock.h"

template<typename ClockT>
class PerformanceTimerChrono {
  public:
    using time_point = typename ClockT::time_point;
    PerformanceTimerChrono()
            : m_startTime(){};

    void start() {
        m_startTime = ClockT::now();
    };

    mixxx::Duration elapsed() const {
        return mixxx::Duration::fromStdDuration(ClockT::now() - m_startTime);
    };
    mixxx::Duration restart() {
        const time_point now = ClockT::now();
        const auto dur = mixxx::Duration::fromStdDuration(now - m_startTime);
        m_startTime = now;
        return dur;
    };

    mixxx::Duration difference(const PerformanceTimerChrono& timer) const {
        return mixxx::Duration::fromStdDuration(m_startTime - timer.m_startTime);
    };

    bool running() const {
        return m_startTime.time_since_epoch().count() != 0;
    };

  private:
    time_point m_startTime;
};

using PerformanceTimer = PerformanceTimerChrono<HighResolutionMonotonicClock>;
