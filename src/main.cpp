#include <iostream>
#include <map>
#include <random>
#include <string>

#include "stadium/config.hpp"
#include "stadium/interactive.hpp"
#include "stadium/simulation.hpp"

namespace {

void usage() {
    std::cout <<
R"(Usage: stadium <mode> [options]

Modes:
  compare       compare gate-selection strategies over many seeded runs (default)
  sim           one detailed simulation report
  interactive   live console with a background clock

Options:
  --capacity N        seats                       (default 100)
  --gates N           entry gates                 (default 10)
  --entry-minutes N   minutes per admission       (default 1)
  --vip-every N       every Nth serial is VIP     (default 12, 0 = none)
  --seed N            RNG seed                    (default: random for interactive, 42 otherwise)
  --strategy S        least-loaded | random | round-robin   (sim only)
  --window N          arrival window in cycles    (default 10)
  --runs N            runs to average in compare  (default 200)
  --tick-ms N         real ms per simulated minute in interactive (default 2000)
)";
}

}  // namespace

int main(int argc, char** argv) {
    std::string mode = "compare";
    int i = 1;
    if (argc > 1 && std::string(argv[1]).rfind("--", 0) != 0) mode = argv[i++];

    std::map<std::string, std::string> opts;
    for (; i < argc; ++i) {
        std::string key = argv[i];
        if (key == "--help" || key == "-h") { usage(); return 0; }
        if (key.rfind("--", 0) != 0 || i + 1 >= argc) { usage(); return 2; }
        opts[key] = argv[++i];
    }

    try {
        auto num = [&](const char* k, int def) { auto it = opts.find(k); return it == opts.end() ? def : std::stoi(it->second); };

        stadium::Config cfg;
        cfg.capacity = num("--capacity", cfg.capacity);
        cfg.gates = num("--gates", cfg.gates);
        cfg.minutesPerEntry = num("--entry-minutes", cfg.minutesPerEntry);
        cfg.vipEvery = num("--vip-every", cfg.vipEvery);

        stadium::SimOptions sim;
        sim.arrivalWindow = num("--window", sim.arrivalWindow);
        sim.seed = static_cast<unsigned>(num("--seed", static_cast<int>(sim.seed)));
        if (auto it = opts.find("--strategy"); it != opts.end()) {
            auto s = stadium::parseStrategy(it->second);
            if (!s) { std::cerr << "Unknown strategy: " << it->second << "\n"; return 2; }
            sim.strategy = *s;
        }

        if (mode == "compare") {
            stadium::printComparison(std::cout, cfg, sim, num("--runs", 200));
        } else if (mode == "sim") {
            stadium::printReport(std::cout, cfg, sim.strategy, stadium::runSimulation(cfg, sim));
        } else if (mode == "interactive") {
            unsigned seed = opts.count("--seed") ? sim.seed : std::random_device{}();
            stadium::runInteractive(cfg, seed, num("--tick-ms", 2000));
        } else {
            usage();
            return 2;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
