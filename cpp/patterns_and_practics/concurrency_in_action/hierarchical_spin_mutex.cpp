#include <cstdint>
#include <atomic>
#include <thread>
#include <stdexcept>
#include <cassert>
#include <mutex>
#include <iostream>

// build with TSAN: clang++ -fsanitize=thread -g -O1 hierarchical_spin_mutex.cpp

// Hierarchical spin mutex is a mutex designed to prevent deadlocks during locking several mutexes. 
// The rule is that if you already have a locked mutex, than you may lock only mutex with lower priority.

class hierarchical_spin_mutex {
    std::size_t mutex_level;
    std::size_t prev_mutex_level;
    std::atomic<bool> is_locked;
    static thread_local std::size_t current_thread_mutex_level;
public:
    hierarchical_spin_mutex(std::size_t level)
        : mutex_level{level}
        , prev_mutex_level{0}
        , is_locked{false}
    {}

    void lock() {
        if (current_thread_mutex_level <= mutex_level) {
            throw std::logic_error("try to acquire mutex with a lower level.");
        }
        bool expected{false};
        while (!is_locked.compare_exchange_weak(expected, true, std::memory_order_acquire, std::memory_order_relaxed)) {
            expected = false;
            std::this_thread::yield();
        }
        prev_mutex_level = current_thread_mutex_level;
        current_thread_mutex_level = mutex_level;
    }

    void unlock() {
        assert(is_locked.load(std::memory_order_relaxed));

        if (current_thread_mutex_level == mutex_level) {
            throw std::logic_error("unlocking order has been violated.");
        }

        current_thread_mutex_level = prev_mutex_level;
        is_locked.exchange(false, std::memory_order_release);
    }

    bool try_lock() {
        if (current_thread_mutex_level <= mutex_level) {
            throw std::logic_error("try to acquire mutex with a lower level.");
        }

        bool expected = false;
        if (is_locked.compare_exchange_strong(expected, true, std::memory_order_acquire, std::memory_order_relaxed)) {
            prev_mutex_level = current_thread_mutex_level;
            current_thread_mutex_level = mutex_level;
            return true;
        }
        return false;
    }
};

thread_local std::size_t hierarchical_spin_mutex::current_thread_mutex_level{std::numeric_limits<std::size_t>::max()};

int main() {
    { // correct test case
        hierarchical_spin_mutex m1{1};
        hierarchical_spin_mutex m2{2};

        std::lock_guard<hierarchical_spin_mutex> lg2(m2);
        std::lock_guard<hierarchical_spin_mutex> lg1(m1);
    }

    { // getting exception while trying to lock the mutex with a lower level
        hierarchical_spin_mutex m1{1};
        hierarchical_spin_mutex m2{2};

        std::lock_guard<hierarchical_spin_mutex> lg1(m1);
        try {
            std::lock_guard<hierarchical_spin_mutex> lg2(m2);
        } catch (std::logic_error e) {
            std::cout << "get the exception: " << e.what() << std::endl;
        }
    }


    { // multi-threaded example
        int protected_data{0};
        hierarchical_spin_mutex m1{1};

        std::thread thr{[&] {
            std::lock_guard<hierarchical_spin_mutex> lg{m1};
            ++protected_data;
        }};

        {
            std::lock_guard<hierarchical_spin_mutex> lg{m1};
            ++protected_data;
        }

        thr.join();
        assert(protected_data == 2);
    }
}
