// Dependency-free test runner so the project builds anywhere. Each TEST is one scenario.
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include "stadium/simulation.hpp"
#include "stadium/stadium.hpp"

using namespace stadium;

static int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) { ++failures; std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; } \
    } while (0)

#define TEST(name) static void name(); struct name##_reg { name##_reg() { registry().push_back({#name, name}); } } name##_inst; static void name()

struct Case { const char* name; void (*fn)(); };
static std::vector<Case>& registry() { static std::vector<Case> r; return r; }

constexpr int BASE = 1'000'000;
static int S(int i) { return BASE + i; }   // serial of index i

TEST(vip_rule_matches_original_project) {
    Stadium st{Config{}};
    int vips = 0;
    for (int i = 0; i < 100; ++i) vips += st.isVip(S(i));
    CHECK(vips == 9);                      // 0,12,...,96
    CHECK(st.regularSerials().size() == 91);
}

TEST(vips_disabled_and_invalid_config) {
    Config c; c.vipEvery = 0;
    Stadium st(c);
    CHECK(st.regularSerials().size() == 100);
    bool threw = false;
    try { Config bad; bad.gates = 0; Stadium s(bad); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
}

TEST(serial_validation) {
    Stadium st{Config{}};
    CHECK(!st.statusOf(BASE - 1));
    CHECK(!st.statusOf(BASE + 100));
    CHECK(st.join(5, 0) == Outcome::InvalidSerial);
    CHECK(st.join(S(1), 10) == Outcome::InvalidGate);
    CHECK(st.join(S(1), -1) == Outcome::InvalidGate);
    CHECK(*st.statusOf(S(1)) == Status::Outside);   // failed join changed nothing
}

TEST(fifo_and_one_admission_per_gate_per_tick) {
    Stadium st{Config{}};
    CHECK(st.join(S(1), 0) == Outcome::Queued);
    CHECK(st.join(S(2), 0) == Outcome::Queued);
    CHECK(st.join(S(3), 1) == Outcome::Queued);
    CHECK(st.tick() == 2);                          // gate 0 and gate 1 each admit one
    CHECK(*st.statusOf(S(1)) == Status::Inside);    // first in, first out
    CHECK(*st.statusOf(S(2)) == Status::Queued);
    CHECK(*st.statusOf(S(3)) == Status::Inside);
    CHECK(st.tick() == 1);
    CHECK(*st.statusOf(S(2)) == Status::Inside);
}

TEST(vip_enters_immediately) {
    Stadium st{Config{}};
    CHECK(st.join(S(12), 3) == Outcome::VipAdmitted);
    CHECK(*st.statusOf(S(12)) == Status::Inside);
    CHECK(st.inside() == 1);
}

TEST(double_join_and_reentry_rejected) {
    Stadium st{Config{}};
    st.join(S(1), 0);
    CHECK(st.join(S(1), 1) == Outcome::AlreadyQueued);
    st.tick();
    CHECK(st.join(S(1), 1) == Outcome::AlreadyInside);
    CHECK(st.switchGate(S(1), 2) == Outcome::AlreadyInside);
}

// Regression for the original bug: a person who switched gates stayed in the old
// queue too, so they were admitted twice and the counter overshot.
TEST(switch_never_duplicates_person) {
    Stadium st{Config{}};
    st.join(S(1), 0);
    st.join(S(2), 0);
    CHECK(st.switchGate(S(2), 4) == Outcome::Switched);
    CHECK(st.queueLength(0) == 1);
    CHECK(st.queueLength(4) == 1);
    CHECK(st.switchGate(S(2), 4) == Outcome::SameGate);
    CHECK(st.switchGate(S(3), 4) == Outcome::NotQueued);
    for (int guard = 0; st.inside() < 2 && guard < 10; ++guard) st.tick();
    CHECK(st.inside() == 2);                        // exactly two people, not three
    st.tick(); st.tick();
    CHECK(st.inside() == 2);
}

TEST(least_loaded_index_tracks_changes) {
    Config c; c.gates = 3;
    Stadium st(c);
    CHECK(st.leastLoadedGates().size() == 3);       // all tied at 0
    st.join(S(1), 0);
    st.join(S(2), 1);
    auto g = st.leastLoadedGates();
    CHECK(g.size() == 1 && g[0] == 2);
    st.join(S(3), 2);
    st.join(S(4), 0);
    g = st.leastLoadedGates();
    CHECK(g.size() == 2);                           // gates 1 and 2 tied at 1
    st.switchGate(S(4), 1);                         // lengths are now 1,2,1
    g = st.leastLoadedGates();
    CHECK(g.size() == 2 && g[0] == 0 && g[1] == 2);
}

TEST(switch_preserves_original_arrival_time) {
    Config c; c.gates = 2;
    Stadium st(c);
    st.join(S(1), 0);
    st.join(S(2), 0);
    st.tick();                                      // S(1) in, wait 1
    st.switchGate(S(2), 1);                         // S(2) moves, but arrived at t=0
    st.tick();                                      // S(2) in at t=2
    CHECK(st.stats().maxWait == 2);
}

TEST(simulation_completes_and_is_deterministic) {
    Config c;
    SimOptions o; o.seed = 123;
    Stats a = runSimulation(c, o), b = runSimulation(c, o);
    CHECK(a.regularAdmitted == 91 && a.vipAdmitted == 9);
    CHECK(a.minutes == b.minutes && a.avgWait == b.avgWait && a.maxWait == b.maxWait);
    std::size_t total = 0;
    for (auto n : a.perGate) total += n;
    CHECK(total == 91);                             // nobody admitted twice or lost
}

TEST(least_loaded_beats_random_on_average) {
    Config c;
    double ll = 0, rnd = 0;
    for (unsigned s = 0; s < 100; ++s) {
        SimOptions o; o.seed = s;
        o.strategy = Strategy::LeastLoaded; ll += runSimulation(c, o).avgWait;
        o.strategy = Strategy::Random;      rnd += runSimulation(c, o).avgWait;
    }
    CHECK(ll < rnd);
}

TEST(scales_to_large_stadium) {
    Config c; c.capacity = 50'000; c.gates = 40; c.vipEvery = 100;
    SimOptions o; o.arrivalWindow = 60;
    Stats s = runSimulation(c, o);
    CHECK(s.regularAdmitted + s.vipAdmitted == 50'000);
}

// Mirrors how interactive.cpp shares a Stadium: one mutex, a ticker thread and a
// "user" thread hammering join/switch. Run under -fsanitize=thread to prove it is race-free.
TEST(concurrent_access_with_mutex) {
    Config c; c.capacity = 400; c.gates = 8; c.vipEvery = 0;
    Stadium st(c);
    std::mutex m;
    std::atomic<bool> stop{false};
    std::thread ticker([&] {
        while (!stop) {
            { std::lock_guard<std::mutex> lk(m); st.tick(); }
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    });
    for (int i = 0; i < 400; ++i) {
        std::lock_guard<std::mutex> lk(m);
        st.join(S(i), i % 8);
        st.switchGate(S(i), (i + 3) % 8);           // may legitimately fail if already inside
    }
    while (true) {
        { std::lock_guard<std::mutex> lk(m); if (st.full()) break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    stop = true;
    ticker.join();
    CHECK(st.inside() == 400);                      // exactly once each, never more
}

int main() {
    for (const auto& c : registry()) {
        int before = failures;
        c.fn();
        std::cout << (failures == before ? "[ ok ] " : "[FAIL] ") << c.name << "\n";
    }
    std::cout << (failures ? "\nSOME TESTS FAILED\n" : "\nAll tests passed\n");
    return failures ? 1 : 0;
}
