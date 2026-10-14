#pragma once

#include "stadium/config.hpp"

namespace stadium {

// Console front-end. A background ticker thread advances virtual time every `tickMs`
// real milliseconds while the main thread reads user input. All access to the
// Stadium is guarded by one mutex; the ticker sleeps on a condition_variable so it
// can be stopped instantly and is always joined (no detached threads).
void runInteractive(const Config& cfg, unsigned seed, int tickMs);

}  // namespace stadium
