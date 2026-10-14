#pragma once

#include <optional>
#include <ostream>
#include <random>
#include <string>
#include <vector>

#include "stadium/stadium.hpp"

namespace stadium {

// How an arriving spectator picks a gate.
enum class Strategy { LeastLoaded, Random, RoundRobin };

std::optional<Strategy> parseStrategy(const std::string& name);
const char* toString(Strategy s);

class GateSelector {
public:
    GateSelector(Strategy s, unsigned seed) : strategy_(s), rng_(seed) {}
    int pick(const Stadium& st);   // returns a 0-based gate index

private:
    Strategy strategy_;
    std::mt19937 rng_;
    int next_ = 0;
};

// Puts `count` randomly chosen regular spectators into queues (the "already waiting
// when gates open" crowd). Uses shuffle instead of rejection sampling, so it always
// terminates. Returns the serials that were queued.
std::vector<int> prefill(Stadium& st, GateSelector& sel, std::size_t count, std::mt19937& rng);

struct SimOptions {
    Strategy strategy = Strategy::LeastLoaded;
    unsigned seed = 42;
    int arrivalWindow = 10;   // remaining spectators arrive uniformly within this many cycles
};

// Runs a full deterministic simulation (no sleeping) and returns the statistics.
Stats runSimulation(const Config& cfg, const SimOptions& opt);

void printReport(std::ostream& os, const Config& cfg, Strategy s, const Stats& st);
void printComparison(std::ostream& os, const Config& cfg, const SimOptions& base, int runs);

}  // namespace stadium
