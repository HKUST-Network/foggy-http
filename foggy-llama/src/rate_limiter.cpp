#include "rate_limiter.h"

namespace foggy_llama {
int ChatRateLimiter::admit(Clock::time_point now) {
    // TODO: Start or renew the window, then admit up to capacity requests.
    // A rejected request keeps the same window and returns a positive delay.
    (void)now;
    return 0;
}
}
