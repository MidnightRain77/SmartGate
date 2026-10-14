#pragma once

#include <cstddef>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "stadium/config.hpp"
#include "stadium/gate.hpp"

namespace stadium {

enum class Status { Outside, Queued, Inside };

enum class Outcome {
    Queued,         // joined a gate queue
    Switched,       // moved to another gate queue
    VipAdmitted,    // VIP went straight in
    InvalidSerial,
    InvalidGate,
    AlreadyInside,
    AlreadyQueued,
    NotQueued,
    SameGate
};

struct Stats {
    int minutes = 0;                    // virtual time elapsed
    std::size_t regularAdmitted = 0;
    std::size_t vipAdmitted = 0;
    double avgWait = 0;                 // minutes, regular spectators only
    int p95Wait = 0;
    int maxWait = 0;
    std::size_t peakQueue = 0;          // longest single queue ever observed
    double utilisation = 0;             // fraction of gate-slots that admitted someone
    std::vector<std::size_t> perGate;   // admissions per gate
};

// Core domain model. Deterministic and single-threaded on purpose:
// time only moves when tick() is called, so a "minute" costs nothing to simulate.
// NOT thread-safe: callers sharing it between threads must hold a mutex
// (see interactive.cpp).
class Stadium {
public:
    explicit Stadium(Config cfg);   // throws std::invalid_argument on bad config

    const Config& config() const { return cfg_; }
    int now() const { return cycle_ * cfg_.minutesPerEntry; }   // virtual minutes
    bool full() const { return inside_ == static_cast<std::size_t>(cfg_.capacity); }
    std::size_t inside() const { return inside_; }

    // Serial <-> index mapping is O(1) arithmetic (no search, no hash needed).
    int serialOf(std::size_t index) const { return cfg_.serialBase + static_cast<int>(index); }
    std::optional<std::size_t> indexOf(int serial) const;

    std::optional<Status> statusOf(int serial) const;
    bool isVip(int serial) const;
    std::vector<int> regularSerials() const;

    // --- queue operations (each is atomic from the caller's point of view) ---
    Outcome join(int serial, int gate);         // gate is 0-based; ignored for VIPs
    Outcome switchGate(int serial, int gate);   // keeps the original arrival time

    // Advance one entry cycle: every gate admits at most one person.
    std::size_t tick();

    // --- inspection ---
    std::size_t queueLength(int gate) const { return gates_[gate].size(); }
    // All gates sharing the minimum queue length. O(k + log N) via the ordered load index.
    std::vector<int> leastLoadedGates() const;
    Stats stats() const;

private:
    struct Person {
        bool vip = false;
        Status status = Status::Outside;
        int gate = -1;
        int joinedAt = 0;   // virtual minute of first arrival (used for wait statistics)
    };

    bool validGate(int g) const { return g >= 0 && g < cfg_.gates; }
    void enqueue(std::size_t idx, int gate);
    void reindex(int gate, std::size_t oldSize);

    Config cfg_;
    std::vector<Person> people_;
    std::vector<Gate> gates_;
    std::set<std::pair<std::size_t, int>> load_;   // (queue length, gate), ordered
    std::vector<int> waits_;
    std::size_t inside_ = 0, vipInside_ = 0, peakQueue_ = 0;
    int cycle_ = 0;
};

}  // namespace stadium
