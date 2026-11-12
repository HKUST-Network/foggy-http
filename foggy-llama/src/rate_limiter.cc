#include "rate_limiter.h"

namespace foggy_llama {
constexpr unsigned ChatRateLimiter::capacity;
constexpr std::chrono::seconds ChatRateLimiter::window;

int ChatRateLimiter::admit(Clock::time_point now) {
    if (used_ == 0 || now - started_at_ >= window) {
        started_at_ = now;
        used_ = 0;
    }
    if (used_ >= capacity) return 1;
    ++used_;
    return 0;
}
}
