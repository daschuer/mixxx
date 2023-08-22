// this implements a std::chrono::Clock that is steady and high resolution
// on all platforms we support

#include <chrono>
#include <type_traits>

class HighResolutionMonotonicClockFallback {
  public:
    using rep = std::chrono::nanoseconds::rep;
    using period = std::chrono::nanoseconds::period;
    using duration = std::chrono::nanoseconds;
    using time_point = std::chrono::time_point<HighResolutionMonotonicClockFallback>;

    static constexpr bool is_steady = true;

    static time_point now() noexcept;
};

using HighResolutionMonotonicClock = ::std::conditional_t<
        ::std::chrono::high_resolution_clock::is_steady,
        ::std::chrono::high_resolution_clock,
        HighResolutionMonotonicClockFallback>;

// TODO: std::chrono::is_clock does not seem to be available on macOS in
// c++2a mode.
// static_assert(std::chrono::is_clock<HighResolutionMonotonicClock>::value);
