#include "stadium/simulation.hpp"

#include <algorithm>
#include <array>
#include <iomanip>

namespace stadium {

std::optional<Strategy> parseStrategy(const std::string& name) {
    if (name == "least-loaded") return Strategy::LeastLoaded;
    if (name == "random") return Strategy::Random;
    if (name == "round-robin") return Strategy::RoundRobin;
    return std::nullopt;
}

const char* toString(Strategy s) {
    switch (s) {
        case Strategy::LeastLoaded: return "least-loaded";
        case Strategy::Random:      return "random";
        case Strategy::RoundRobin:  return "round-robin";
    }
    return "?";
}

int GateSelector::pick(const Stadium& st) {
    const int n = st.config().gates;
    switch (strategy_) {
        case Strategy::LeastLoaded: {
            auto best = st.leastLoadedGates();   // break ties randomly, not always gate 1
            std::uniform_int_distribution<std::size_t> d(0, best.size() - 1);
            return best[d(rng_)];
        }
        case Strategy::Random: {
            std::uniform_int_distribution<int> d(0, n - 1);   // unbiased, unlike rand() % n
            return d(rng_);
        }
        case Strategy::RoundRobin: {
            int g = next_;
            next_ = (next_ + 1) % n;
            return g;
        }
    }
    return 0;
}

std::vector<int> prefill(Stadium& st, GateSelector& sel, std::size_t count, std::mt19937& rng) {
    auto candidates = st.regularSerials();
    std::shuffle(candidates.begin(), candidates.end(), rng);
    candidates.resize(std::min(count, candidates.size()));
    for (int serial : candidates) st.join(serial, sel.pick(st));
    return candidates;
}

Stats runSimulation(const Config& cfg, const SimOptions& opt) {
    Stadium st(cfg);
    std::mt19937 rng(opt.seed);
    GateSelector selector(opt.strategy, opt.seed + 1);   // separate stream: same crowd for every strategy

    prefill(st, selector, static_cast<std::size_t>(cfg.capacity) / 2, rng);

    const int window = std::max(0, opt.arrivalWindow);
    std::vector<std::vector<int>> arrivals(window + 1);
    std::uniform_int_distribution<int> when(0, window);
    for (int i = 0; i < cfg.capacity; ++i) {
        int serial = st.serialOf(i);
        if (*st.statusOf(serial) == Status::Outside) arrivals[when(rng)].push_back(serial);
    }

    for (std::size_t cycle = 0; !st.full(); ++cycle) {
        if (cycle < arrivals.size())
            for (int serial : arrivals[cycle])
                st.join(serial, st.isVip(serial) ? 0 : selector.pick(st));
        st.tick();
    }
    return st.stats();
}

void printReport(std::ostream& os, const Config& cfg, Strategy s, const Stats& st) {
    os << "=== Simulation: strategy=" << toString(s) << ", capacity=" << cfg.capacity
       << ", gates=" << cfg.gates << " ===\n"
       << std::fixed << std::setprecision(2)
       << "Time to fill stadium     : " << st.minutes << " min\n"
       << "Admitted (regular / VIP) : " << st.regularAdmitted << " / " << st.vipAdmitted << "\n"
       << "Average wait             : " << st.avgWait << " min\n"
       << "95th percentile wait     : " << st.p95Wait << " min\n"
       << "Maximum wait             : " << st.maxWait << " min\n"
       << "Peak single-queue length : " << st.peakQueue << "\n"
       << "Gate utilisation         : " << st.utilisation * 100 << " %\n"
       << "Admissions per gate      :";
    for (auto n : st.perGate) os << ' ' << n;
    os << "\n";
}

void printComparison(std::ostream& os, const Config& cfg, const SimOptions& base, int runs) {
    constexpr std::array<Strategy, 3> all = {Strategy::LeastLoaded, Strategy::RoundRobin, Strategy::Random};
    os << "Averages over " << runs << " seeded runs (capacity=" << cfg.capacity
       << ", gates=" << cfg.gates << ", arrival window=" << base.arrivalWindow << ")\n\n"
       << std::left << std::setw(14) << "strategy" << std::right
       << std::setw(10) << "total min" << std::setw(10) << "avg wait" << std::setw(10) << "p95 wait"
       << std::setw(10) << "max wait" << std::setw(12) << "peak queue" << "\n";

    for (Strategy s : all) {
        double total = 0, avg = 0, p95 = 0, mx = 0, peak = 0;
        for (int r = 0; r < runs; ++r) {
            SimOptions o = base;
            o.strategy = s;
            o.seed = base.seed + static_cast<unsigned>(r);
            Stats st = runSimulation(cfg, o);
            total += st.minutes; avg += st.avgWait; p95 += st.p95Wait;
            mx += st.maxWait;    peak += static_cast<double>(st.peakQueue);
        }
        os << std::left << std::setw(14) << toString(s) << std::right << std::fixed << std::setprecision(1)
           << std::setw(10) << total / runs << std::setw(10) << avg / runs << std::setw(10) << p95 / runs
           << std::setw(10) << mx / runs << std::setw(12) << peak / runs << "\n";
    }
}

}  // namespace stadium
