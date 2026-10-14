#pragma once

#include <cstddef>
#include <iterator>
#include <list>
#include <optional>
#include <unordered_map>

namespace stadium {

// A FIFO queue with O(1) push, pop and *removal of an arbitrary member*.
// std::list gives stable iterators; the hash map remembers where each id lives.
class Gate {
public:
    void push(int id) {
        queue_.push_back(id);
        pos_[id] = std::prev(queue_.end());
    }

    bool remove(int id) {
        auto it = pos_.find(id);
        if (it == pos_.end()) return false;
        queue_.erase(it->second);
        pos_.erase(it);
        return true;
    }

    std::optional<int> pop() {
        if (queue_.empty()) return std::nullopt;
        int id = queue_.front();
        queue_.pop_front();
        pos_.erase(id);
        ++admitted_;
        return id;
    }

    std::size_t size() const { return queue_.size(); }
    std::size_t admitted() const { return admitted_; }

private:
    std::list<int> queue_;
    std::unordered_map<int, std::list<int>::iterator> pos_;
    std::size_t admitted_ = 0;
};

}  // namespace stadium
