#include "util/performancetimer.h"

#include <benchmark/benchmark.h>
#include <gtest/gtest.h>

#include <cmath>
#include <ctime>
#include <iostream>

#include "util/performancetimerlegacy.h"

// This test was added because of an signed/unsigned underflow bug that
// affected Windows and (presumably) Symbian.
// See https://github.com/mixxxdj/mixxx/issues/7397
TEST(PerformanceTimerTest, DifferenceCanBeNegative) {
    PerformanceTimer early;
    early.start();

    std::time_t start = time(0);
    while (1) {
        // use the standard clock to make sure we don't run forever
        double seconds = difftime(time(0), start);

        PerformanceTimer late;
        late.start();
        mixxx::Duration difference = early.difference(late);

        // If the high-res clock hasn't ticked yet, the difference should be 0.
        // If it has ticked, then the difference better be negative.
        ASSERT_LE(difference.toIntegerNanos(), 0);

        if (difference < mixxx::Duration::fromNanos(0)) {
            break; // test passed
        }

        if (seconds > 0.9) {
            // The standard clock ticked, but difference is still zero.
            // Assume that there is no high-resolution clock on this system.
            ASSERT_EQ(0, difference.toIntegerNanos());

            // It would be nice if gtest printed this, but it currently doesn't.
            // https://code.google.com/p/googletest/wiki/AdvancedGuide
            SUCCEED() << "inconclusive: no high-resolution timer on this system?";
            break;
        }
    }
}

// Benchmark that measures the overhead of PerformanceTimerLegacy vs PerformanceTimerChrono<T>

template<typename Timer>
void BM_PerformanceTimer(benchmark::State& state) {
    Timer timer;
    for (auto _ : state) {
        // I assume that starting the timer will only start the timer
        // without (unnecessarily) calculating a duration. Also
        // that its valid to call start() multiple times.
        for (int i = 0; i < 100; ++i) {
            timer.start();
        }
    }
}
// TODO: use templated benchmark instead; this resulted in a compile error
// on my end I couldn't figure out.
static void BM_PerformanceTimerLegacy(benchmark::State& state) {
    BM_PerformanceTimer<PerformanceTimerLegacy>(state);
}

BENCHMARK(BM_PerformanceTimerLegacy);

static void BM_PerformanceTimerChronoStdSteady(benchmark::State& state) {
    BM_PerformanceTimer<PerformanceTimerChrono<std::chrono::steady_clock>>(state);
}

BENCHMARK(BM_PerformanceTimerChronoStdSteady);

static void BM_PerformanceTimerChronoStdHighRes(benchmark::State& state) {
    BM_PerformanceTimer<PerformanceTimerChrono<std::chrono::high_resolution_clock>>(state);
}

BENCHMARK(BM_PerformanceTimerChronoStdHighRes);

static void BM_PerformanceTimerChronoHighResMonotonic(benchmark::State& state) {
    BM_PerformanceTimer<PerformanceTimerChrono<HighResolutionMonotonicClock>>(state);
}

BENCHMARK(BM_PerformanceTimerChronoHighResMonotonic);

volatile int exti = 0;

static void BM_PerformanceTimerNull(benchmark::State& state) {
    for (auto _ : state) {
        // I assume that starting the timer will only start the timer
        // without (unnecessarily) calculating a duration. Also
        // that its valid to call start() multiple times.
        for (int i = 0; i < 100; ++i) {
            exti = exti + 1;
        }
    }
}

BENCHMARK(BM_PerformanceTimerNull);

// BENCHMARK(BM_PerformanceTimer<PerformanceTimerLegacy>)
// BENCHMARK(BM_PerformanceTimer<PerformanceTimerChrono<std::chrono::steady_clock>>)
// BENCHMARK(BM_PerformanceTimer<PerformanceTimerChrono<std::chrono::high_resolution_clock>>)
// BENCHMARK(BM_PerformanceTimer<PerformanceTimerChrono<HighResolutionMonotonicClock>>)
