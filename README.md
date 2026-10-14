<![CDATA[# SmartGate — Stadium Entry Queue Simulator

> A high-performance C++17 discrete-event simulator and live console for managing spectators entering a stadium through multiple gates, with VIP fast-lane, gate switching, and a head-to-head comparison of gate-assignment strategies.

---

## ✨ Features

- **Three operating modes** — automated strategy comparison, single detailed simulation, and a real-time interactive console.
- **Gate-assignment strategies** — *Least-Loaded*, *Round-Robin*, and *Random*; compare them side-by-side across hundreds of seeded runs.
- **VIP fast-lane** — every *k*-th ticket holder bypasses all queues and enters immediately through a dedicated gate.
- **Gate switching** — spectators already in a queue can move to a different gate at any time; the system preserves their original arrival time for fair wait statistics.
- **Virtual clock** — the deterministic core never sleeps or prints, so a 10-minute scenario completes in microseconds and the entire domain layer is trivially testable.
- **Thread-safe interactive mode** — a background ticker thread advances simulated time while the main thread reads user commands, all synchronized via a single mutex and condition variable.
- **Seedable, reproducible randomness** — uses `std::mt19937` + `std::uniform_int_distribution` for unbiased, repeatable results.
- **13 automated tests** — dependency-free runner, including a concurrent stress test designed for ThreadSanitizer.

---

## 📋 Prerequisites

| Tool | Minimum version |
|------|-----------------|
| C++ compiler | C++17 support (`g++`, `clang++`, or MSVC) |
| CMake | 3.16+ |
| pthreads | (bundled on Linux/macOS) |

---

## 🚀 Quick Start

### Build

```bash
cmake -S . -B build && cmake --build build
```

### Run tests

```bash
ctest --test-dir build --output-on-failure
```

### Run the simulator

```bash
# Compare all three strategies over 200 seeded runs (default)
./build/stadium compare

# One detailed report for a specific strategy + seed
./build/stadium sim --seed 7 --strategy random

# Interactive console (1 simulated minute = 2 real seconds)
./build/stadium interactive --tick-ms 2000

# Custom scenario: 5,000 seats, 20 gates, VIP every 50th ticket, 30-cycle arrival window
./build/stadium compare --capacity 5000 --gates 20 --vip-every 50 --window 30
```

### Without CMake

```bash
g++ -std=c++17 -O2 -Iinclude src/*.cpp -pthread -o stadium
```

### Sanitizer builds

```bash
cmake -S . -B build-tsan -DSANITIZE=thread    # also: address, undefined
cmake --build build-tsan
```

---

## 📊 Sample Output

```
Averages over 200 seeded runs (capacity=100, gates=10, arrival window=10)

strategy       total min  avg wait  p95 wait  max wait  peak queue
least-loaded        11.0       3.0       5.4       6.0         6.0
round-robin         11.0       3.0       5.4       6.0         6.0
random              14.1       3.5       7.8       9.4         9.4
```

Balanced assignment (**least-loaded**) cuts the time to fill the stadium by **~22 %** and the worst-case wait by **~36 %** versus random choice. Round-robin ties least-loaded here because all gates have identical speed; least-loaded additionally adapts when queues become uneven (e.g. after gate switches).

---

## 🗂️ Project Structure

```
SmartGate/
├── CMakeLists.txt                # Build configuration
├── include/stadium/              # Public headers
│   ├── config.hpp                #   Config struct (capacity, gates, entry time, VIP rule)
│   ├── gate.hpp                  #   Gate: FIFO queue with O(1) arbitrary removal
│   ├── stadium.hpp               #   Stadium: domain model + virtual clock (no I/O, no threads)
│   ├── simulation.hpp            #   Strategies, auto-arrivals, statistics, reports
│   └── interactive.hpp           #   Console front-end with background clock thread
├── src/
│   ├── main.cpp                  # CLI entry point (compare | sim | interactive)
│   ├── stadium.cpp               # Stadium implementation
│   ├── simulation.cpp            # Simulation engine & strategy comparison
│   └── interactive.cpp           # Interactive mode (multi-threaded console)
└── tests/
    └── tests.cpp                 # 13 tests, dependency-free runner, registered with CTest
```

**Layering:** `Gate` → `Stadium` *(pure logic)* → `simulation` / `interactive` *(I/O and policy)*

The core never prints or sleeps — that is what makes it testable and fast.

---

## 🏗️ Architecture & Design Decisions

| Concern | Approach | Complexity |
|---------|----------|------------|
| Queue with arbitrary removal (gate switching) | `std::list` + `unordered_map<id, iterator>` | push / pop / remove: **O(1)** |
| Find the least-loaded gate | `std::set<(length, gate)>` kept in sync on every change | query **O(k + log N)**, update **O(log N)** |
| Serial ↔ index mapping | `serial = base + index`, plain arithmetic | **O(1)** |
| Time | Virtual clock advanced by `tick()` | A 10-min run takes μs |
| Randomness | `std::mt19937` + `uniform_int_distribution` (seedable) | Reproducible with `--seed` |
| Pre-filled crowd | `std::shuffle` of candidates | Always terminates |

---

## 🔒 Concurrency Model (Interactive Mode)

- **One `std::mutex`** guards the `Stadium`; both the ticker thread and the input thread acquire it.
- The ticker sleeps on `condition_variable::wait_for` with a stop predicate, so **shutdown is immediate**. The thread is joined via an **RAII guard** (no `detach`).
- **No check-then-act race:** the user may take 30 seconds to pick a gate. `join()` and `switchGate()` re-validate state under the lock and return an `Outcome` (e.g. `AlreadyInside`) instead of trusting stale data.
- Input uses `getline` + `stoi` with full-string validation, so bad input cannot wedge `cin`.
- A stress test (`concurrent_access_with_mutex`) and the interactive binary were both run under **ThreadSanitizer** with zero reports.

---

## ⚙️ CLI Reference

```
Usage: stadium <mode> [options]

Modes:
  compare       Compare gate-selection strategies over many seeded runs (default)
  sim           One detailed simulation report
  interactive   Live console with a background clock

Options:
  --capacity N        Stadium seats                     (default: 100)
  --gates N           Number of entry gates             (default: 10)
  --entry-minutes N   Minutes per admission             (default: 1)
  --vip-every N       Every Nth serial is VIP           (default: 12, 0 = none)
  --seed N            RNG seed                          (default: random for interactive, 42 otherwise)
  --strategy S        least-loaded | random | round-robin  (sim mode only)
  --window N          Arrival window in cycles          (default: 10)
  --runs N            Runs to average in compare mode   (default: 200)
  --tick-ms N         Real ms per simulated minute      (default: 2000, interactive only)
```