#include <cstdint>
#include <atomic>
#include <thread>
#include <stdexcept>
#include <cassert>
#include <mutex>
#include <iostream>

class hierarchical_mutex {
    std::size_t mutex_level;
    std::size_t prev_mutex_level;
    std::atomic<bool> is_locked;
    static thread_local std::size_t current_thread_mutex_level;
    static thread_local std::size_t locking_depth;
public:
    hierarchical_mutex(std::size_t level)
        : mutex_level{level}
        , prev_mutex_level{0}
        , is_locked{false}
    {}

    void lock() {
        if (locking_depth != 0 && current_thread_mutex_level < mutex_level) {
            throw std::runtime_error("try to acquire mutex with a lower level.");
        }
        bool expected{false};
        while (!is_locked.compare_exchange_weak(expected, true, std::memory_order_acquire, std::memory_order_relaxed)) {
            expected = false;
            std::this_thread::yield();
        }
        prev_mutex_level = current_thread_mutex_level;
        current_thread_mutex_level = mutex_level;
        ++locking_depth;
    }

    void unlock() {
        assert(is_locked.load(std::memory_order_relaxed));

        is_locked.exchange(true, std::memory_order_release);
        current_thread_mutex_level = prev_mutex_level;
        --locking_depth;
    }

    bool try_lock() {
        if (locking_depth != 0 && current_thread_mutex_level < mutex_level) {
            throw std::runtime_error("try to acquire mutex with a lower level.");
        }

        bool expected = false;
        if (is_locked.compare_exchange_strong(expected, true, std::memory_order_acquire, std::memory_order_relaxed)) {
            prev_mutex_level = current_thread_mutex_level;
            current_thread_mutex_level = mutex_level;
            ++locking_depth;
            return true;
        }
        return false;
    }
};

thread_local std::size_t hierarchical_mutex::current_thread_mutex_level{0};
thread_local std::size_t hierarchical_mutex::locking_depth{0};


int main() {
    { // correct test case
        hierarchical_mutex m1{1};
        hierarchical_mutex m2{2};

        std::lock_guard<hierarchical_mutex> lg2(m2);
        std::lock_guard<hierarchical_mutex> lg1(m1);
    }

    { // getting exception while trying to lock the mutex with a lower level
        hierarchical_mutex m1{1};
        hierarchical_mutex m2{2};

        std::lock_guard<hierarchical_mutex> lg1(m1);
        try {
            std::lock_guard<hierarchical_mutex> lg2(m2);
        } catch (std::runtime_error e) {
            std::cout << "get the exception: " << e.what() << std::endl;
        }
    }
}
