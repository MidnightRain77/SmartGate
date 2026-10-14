#pragma once

namespace stadium {

// All tunable parameters live here instead of being scattered as globals.
struct Config {
    int capacity        = 100;        // M: seats in the stadium
    int gates           = 10;         // N: regular entry gates
    int minutesPerEntry = 1;          // p: minutes for one spectator to pass a gate
    int vipEvery        = 12;         // every k-th serial is a VIP (<= 0 disables VIPs)
    int serialBase      = 1'000'000;  // serial numbers are serialBase + [0, capacity)
};

}  // namespace stadium
