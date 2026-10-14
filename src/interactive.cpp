#include "stadium/interactive.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>

#include "stadium/simulation.hpp"
#include "stadium/stadium.hpp"

namespace stadium {
namespace {

struct Shared {
    explicit Shared(const Config& c) : st(c) {}
    std::mutex m;
    std::condition_variable cv;
    bool stop = false;               // guarded by m
    std::atomic<bool> full{false};
    Stadium st;                      // guarded by m
};

void tickerLoop(Shared& sh, std::chrono::milliseconds period) {
    std::unique_lock<std::mutex> lk(sh.m);
    while (!sh.stop) {
        // Releases the lock while sleeping; wakes early only if stop becomes true.
        if (sh.cv.wait_for(lk, period, [&] { return sh.stop; })) break;
        sh.st.tick();
        if (sh.st.full()) {
            sh.full = true;
            std::cout << "\n*** Stadium is full after " << sh.st.now()
                      << " minutes. Press Enter to exit.\n" << std::flush;
            break;
        }
    }
}

// RAII: always stops and joins the ticker, even if main throws.
struct TickerGuard {
    Shared& sh;
    std::thread t;
    ~TickerGuard() {
        { std::lock_guard<std::mutex> lk(sh.m); sh.stop = true; }
        sh.cv.notify_all();
        if (t.joinable()) t.join();
    }
};

std::string trim(const std::string& s) {
    auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

std::optional<int> parseInt(const std::string& s) {
    try {
        std::size_t used = 0;
        int v = std::stoi(s, &used);
        if (used != s.size()) return std::nullopt;   // reject "12abc"
        return v;
    } catch (...) {
        return std::nullopt;                         // not a number / out of range
    }
}

// getline-based input: bad input can never leave the stream in a failed state.
std::optional<std::string> readLine(const std::string& prompt) {
    std::cout << prompt << std::flush;
    std::string line;
    if (!std::getline(std::cin, line)) return std::nullopt;
    return trim(line);
}

std::optional<int> askGate(int gates) {
    while (true) {
        auto line = readLine("Gate number (blank to cancel): ");
        if (!line || line->empty()) return std::nullopt;
        auto v = parseInt(*line);
        if (v && *v >= 1 && *v <= gates) return *v - 1;
        std::cout << "Please enter a number between 1 and " << gates << ".\n";
    }
}

std::string suggestion(const Stadium& st) {
    auto gates = st.leastLoadedGates();
    std::ostringstream os;
    os << "Estimated wait at the shortest queue: "
       << (st.queueLength(gates.front()) + 1) * st.config().minutesPerEntry << " min\n"
       << "Gates with the shortest queue:";
    for (int g : gates) os << ' ' << g + 1;
    os << "\nYou can choose any gate from 1 to " << st.config().gates << ".";
    return os.str();
}

void printStatus(Shared& sh) {
    std::lock_guard<std::mutex> lk(sh.m);
    const Stadium& st = sh.st;
    std::cout << "Time " << st.now() << " min | inside " << st.inside() << "/" << st.config().capacity
              << " | queue lengths:";
    for (int g = 0; g < st.config().gates; ++g) std::cout << ' ' << st.queueLength(g);
    std::cout << "\n";
}

void handleSerial(Shared& sh, int serial) {
    std::optional<Status> status;
    bool vip = false;
    std::string hint;
    int gates = 0;
    {
        std::lock_guard<std::mutex> lk(sh.m);
        status = sh.st.statusOf(serial);
        vip = sh.st.isVip(serial);
        hint = suggestion(sh.st);
        gates = sh.st.config().gates;
    }
    if (!status) {
        std::cout << "Invalid serial number. Please enter a valid 7-digit serial.\n";
        return;
    }
    if (*status == Status::Inside) {
        std::cout << "You have already entered the stadium. Re-entry is not permitted.\n";
        return;
    }
    if (vip && *status == Status::Outside) {
        std::lock_guard<std::mutex> lk(sh.m);
        sh.st.join(serial, 0);
        std::cout << "Welcome, VIP! Please use the exclusive entry gate.\n";
        return;
    }

    if (*status == Status::Queued) std::cout << "You're already in a queue; you can switch gates.\n";
    std::cout << hint << "\n";
    auto gate = askGate(gates);
    if (!gate) { std::cout << "No changes made.\n"; return; }

    // The decision above may have taken a while; the ticker kept running. join/switchGate
    // re-validate under the lock, so there is no check-then-act race.
    std::lock_guard<std::mutex> lk(sh.m);
    Outcome o = (*status == Status::Queued) ? sh.st.switchGate(serial, *gate) : sh.st.join(serial, *gate);
    const int wait = static_cast<int>(sh.st.queueLength(*gate)) * sh.st.config().minutesPerEntry;
    switch (o) {
        case Outcome::Queued:
            std::cout << "You are in the queue for gate " << *gate + 1 << " (about " << wait << " min).\n"; break;
        case Outcome::Switched:
            std::cout << "Moved to gate " << *gate + 1 << " (about " << wait << " min).\n"; break;
        case Outcome::SameGate:
            std::cout << "You are already in that queue.\n"; break;
        case Outcome::AlreadyInside:
            std::cout << "Good news: you were just admitted while choosing!\n"; break;
        default:
            std::cout << "Could not complete that request.\n"; break;
    }
}

}  // namespace

void runInteractive(const Config& cfg, unsigned seed, int tickMs) {
    Shared sh(cfg);
    {
        std::mt19937 rng(seed);
        GateSelector sel(Strategy::LeastLoaded, seed + 1);
        prefill(sh.st, sel, static_cast<std::size_t>(cfg.capacity) / 2, rng);
    }

    std::cout << "Entry Queue Management System  (1 simulated minute = " << tickMs << " ms)\n"
              << "Type a serial (" << cfg.serialBase << "-" << cfg.serialBase + cfg.capacity - 1
              << "), 'status', or 'quit'.\n";

    TickerGuard guard{sh, std::thread(tickerLoop, std::ref(sh), std::chrono::milliseconds(tickMs))};

    while (!sh.full) {
        auto line = readLine("\nSerial number: ");
        if (!line || sh.full) break;
        if (*line == "quit") break;
        if (*line == "status") { printStatus(sh); continue; }
        auto serial = parseInt(*line);
        if (!serial) { std::cout << "Please enter digits only (or 'status' / 'quit').\n"; continue; }
        handleSerial(sh, *serial);
    }

    std::lock_guard<std::mutex> lk(sh.m);
    std::cout << "Session ended at " << sh.st.now() << " simulated minutes with "
              << sh.st.inside() << "/" << cfg.capacity << " inside.\n";
}

}  // namespace stadium
