#include "util/highresolution_monotonic_clock.h"

#include <gtest/gtest.h>

using namespace std::chrono_literals;
constexpr int kNumSamples = 1000;
constexpr std::chrono::nanoseconds kMinResolution = 50ns;

static std::vector<HighResolutionMonotonicClock::duration> generateSamples() {
    using tp = HighResolutionMonotonicClock::time_point;
    std::vector<HighResolutionMonotonicClock::duration> v;
    v.reserve(kNumSamples);
    for (int i = 0; i < kNumSamples; ++i) {
        tp t1 = HighResolutionMonotonicClock::now();
        tp t2 = HighResolutionMonotonicClock::now();
        v.push_back(t2 - t1);
    }
    return v;
}

class HighresolutionMonotonicClockTest : public testing::Test {
};

TEST_F(HighresolutionMonotonicClockTest, sufficientResolution) {
    auto samples = generateSamples();
    auto mid = samples.begin() + (samples.size() / 2);
    std::nth_element(samples.begin(), mid, samples.end());
    ASSERT_LE(*mid, kMinResolution);
}

TEST_F(HighresolutionMonotonicClockTest, monotonic) {
    auto samples = generateSamples();
    ASSERT_TRUE(std::all_of(samples.begin(), samples.end(), [](auto a) { return a >= 0ns; }));
}
