#include "stadium/stadium.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace stadium {

Stadium::Stadium(Config cfg) : cfg_(cfg) {
    if (cfg_.capacity <= 0 || cfg_.gates <= 0 || cfg_.minutesPerEntry <= 0)
        throw std::invalid_argument("capacity, gates and minutesPerEntry must be positive");

    people_.resize(cfg_.capacity);
    for (int i = 0; i < cfg_.capacity; ++i)
        people_[i].vip = cfg_.vipEvery > 0 && i % cfg_.vipEvery == 0;

    gates_.resize(cfg_.gates);
    for (int g = 0; g < cfg_.gates; ++g) load_.emplace(0, g);
}

std::optional<std::size_t> Stadium::indexOf(int serial) const {
    long long idx = static_cast<long long>(serial) - cfg_.serialBase;
    if (idx < 0 || idx >= cfg_.capacity) return std::nullopt;
    return static_cast<std::size_t>(idx);
}

std::optional<Status> Stadium::statusOf(int serial) const {
    auto idx = indexOf(serial);
    if (!idx) return std::nullopt;
    return people_[*idx].status;
}

bool Stadium::isVip(int serial) const {
    auto idx = indexOf(serial);
    return idx && people_[*idx].vip;
}

std::vector<int> Stadium::regularSerials() const {
    std::vector<int> out;
    for (std::size_t i = 0; i < people_.size(); ++i)
        if (!people_[i].vip) out.push_back(serialOf(i));
    return out;
}

void Stadium::reindex(int gate, std::size_t oldSize) {
    load_.erase({oldSize, gate});
    load_.emplace(gates_[gate].size(), gate);
}

void Stadium::enqueue(std::size_t idx, int gate) {
    std::size_t old = gates_[gate].size();
    gates_[gate].push(static_cast<int>(idx));
    reindex(gate, old);
    people_[idx].status = Status::Queued;
    people_[idx].gate = gate;
    peakQueue_ = std::max(peakQueue_, gates_[gate].size());
}

Outcome Stadium::join(int serial, int gate) {
    auto idx = indexOf(serial);
    if (!idx) return Outcome::InvalidSerial;
    Person& p = people_[*idx];
    if (p.status == Status::Inside) return Outcome::AlreadyInside;
    if (p.status == Status::Queued) return Outcome::AlreadyQueued;

    if (p.vip) {                       // dedicated VIP gate: no queue
        p.joinedAt = now();
        p.status = Status::Inside;
        ++inside_;
        ++vipInside_;
        return Outcome::VipAdmitted;
    }
    if (!validGate(gate)) return Outcome::InvalidGate;
    p.joinedAt = now();
    enqueue(*idx, gate);
    return Outcome::Queued;
}

Outcome Stadium::switchGate(int serial, int gate) {
    auto idx = indexOf(serial);
    if (!idx) return Outcome::InvalidSerial;
    Person& p = people_[*idx];
    if (p.status == Status::Inside) return Outcome::AlreadyInside;
    if (p.status != Status::Queued) return Outcome::NotQueued;
    if (!validGate(gate)) return Outcome::InvalidGate;
    if (gate == p.gate) return Outcome::SameGate;

    int from = p.gate;
    std::size_t old = gates_[from].size();
    gates_[from].remove(static_cast<int>(*idx));   // O(1)
    reindex(from, old);
    enqueue(*idx, gate);                           // goes to the back of the new queue
    return Outcome::Switched;
}

std::size_t Stadium::tick() {
    ++cycle_;
    const int t = now();
    std::size_t admitted = 0;
    for (int g = 0; g < cfg_.gates; ++g) {
        std::size_t old = gates_[g].size();
        auto id = gates_[g].pop();
        if (!id) continue;
        reindex(g, old);
        Person& p = people_[*id];
        p.status = Status::Inside;
        p.gate = -1;
        ++inside_;
        waits_.push_back(t - p.joinedAt);
        ++admitted;
    }
    return admitted;
}

std::vector<int> Stadium::leastLoadedGates() const {
    std::vector<int> out;
    const std::size_t best = load_.begin()->first;
    for (auto it = load_.begin(); it != load_.end() && it->first == best; ++it)
        out.push_back(it->second);
    return out;
}

Stats Stadium::stats() const {
    Stats s;
    s.minutes = now();
    s.regularAdmitted = waits_.size();
    s.vipAdmitted = vipInside_;
    s.peakQueue = peakQueue_;
    for (const Gate& g : gates_) s.perGate.push_back(g.admitted());

    if (!waits_.empty()) {
        std::vector<int> w = waits_;
        std::sort(w.begin(), w.end());
        double sum = 0;
        for (int x : w) sum += x;
        s.avgWait = sum / static_cast<double>(w.size());
        s.maxWait = w.back();
        auto k = static_cast<std::size_t>(std::ceil(0.95 * static_cast<double>(w.size())));
        s.p95Wait = w[k - 1];
    }
    if (cycle_ > 0)
        s.utilisation = static_cast<double>(waits_.size()) /
                        (static_cast<double>(cfg_.gates) * cycle_);
    return s;
}

}  // namespace stadium
