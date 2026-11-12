#pragma once
#include <chrono>

namespace foggy_llama {
class ChatRateLimiter {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr unsigned capacity = 3;
    static constexpr std::chrono::seconds window{10};

    // Return 0 to admit and count the request, or a positive value to reject it.
    // Calls use a monotonic clock.
    int admit(Clock::time_point now);

private:
    Clock::time_point started_at_{};
    unsigned used_ = 0;
};
}
