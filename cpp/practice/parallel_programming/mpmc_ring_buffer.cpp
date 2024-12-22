#include <atomic>
#include <vector>

// clang++ mpmc_ring_buffer.cpp -fsanitize=thread -g -lpthread -std=c++17

// Kudos to https://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue for inspiration
// Still need to check using tests and TSAN

template<typename T>
class mpmc_ring_buffer {
    struct buffer_cell {
        std::atomic<std::size_t> epoch;
        T element;
    };

    static constexpr std::size_t cache_line = 64;

    std::vector<buffer_cell> buffer;
    alignas(cache_line) std::atomic<std::size_t> enqueue_pos;
    alignas(cache_line) std::atomic<std::size_t> dequeue_pos;

public:
    mpmc_ring_buffer(std::size_t input_size) 
        : buffer(input_size)
        , enqueue_pos{0}
        , dequeue_pos{0}
    {}

    bool enqueue(const T& el) {
        std::size_t buf_pos, pos_epoch;
        do {
            std::size_t pos = enqueue_pos.load(std::memory_order_relaxed);
            buf_pos = pos % buffer.size();
            pos_epoch = pos / buffer.size();

            std::size_t cell_epoch = buffer[buf_pos].epoch.load(std::memory_order_acquire);
            if (pos_epoch == cell_epoch) {
                if (enqueue_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;
            } else {
                return false;
            }
        } while (true);

        buffer[buf_pos].element = el;
        buffer[buf_pos].epoch.store(pos_epoch + 2, std::memory_order_release);

        return true;
    }

    bool dequeue(T& el) {
        std::size_t buf_pos, pos_epoch;
        do {
            std::size_t pos = dequeue_pos.load(std::memory_order_relaxed);
            buf_pos = pos % buffer.size();
            pos_epoch = pos / buffer.size();
    
            std::size_t cell_epoch = buffer[buf_pos].epoch.load(std::memory_order_acquire);
            if (pos_epoch == (cell_epoch + 2)) {
                if (dequeue_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;
            } else {
                return false;
            }
        } while (true);

        el = buffer[buf_pos].element;
        buffer[buf_pos].epoch.store(pos_epoch + 1, std::memory_order_release);

        return true;
    }
};

#include <thread>
#include <unordered_set>
#include <iostream>
#include <functional>

constexpr std::size_t buffer_size      = 1000;
constexpr std::size_t iterations_count = 1000000;
constexpr std::size_t threads_num      = 10;

using data_type = std::size_t;
using account_table = std::unordered_set<data_type>;

bool is_writer(std::size_t thread_idx) {
    return thread_idx % 2;
}

int main() {
    mpmc_ring_buffer<data_type> buffer(buffer_size);

    account_table common_enq_acc_table, common_deq_acc_table;
    std::vector<account_table> thread_acc_tables(threads_num);
    std::vector<std::thread> thread_pool(threads_num);

    for (std::size_t thread_idx = 0; thread_idx < threads_num; ++thread_idx) {
        thread_pool[thread_idx] = std::thread([&, thread_idx]() {
            data_type base_value = thread_idx * iterations_count;
            data_type value;
            std::function<bool(mpmc_ring_buffer<data_type>&, data_type&)> func;
            if (is_writer(thread_idx)) {
                func = [](mpmc_ring_buffer<data_type>& buffer, data_type& value) { return buffer.enqueue(value); };
            } else {
                func = [](mpmc_ring_buffer<data_type>& buffer, data_type& value) { return buffer.dequeue(value); };
            }

            for (std::size_t i = 0; i < iterations_count; ++i) {
                value = base_value + i;
                if (func(buffer, value)) {
                    if (!thread_acc_tables[thread_idx].insert(value).second) {
                        std::cout << "Value " << value << " already exists in " << thread_idx << " thread table" << std::endl;
                    }
                }
            }
        });
    }
    

    for (std::size_t thread_idx = 0; thread_idx < threads_num; ++thread_idx) {
        thread_pool[thread_idx].join();

        auto& common_table = is_writer(thread_idx) ? common_enq_acc_table : common_deq_acc_table;
        common_table.merge(thread_acc_tables[thread_idx]);
        if (!thread_acc_tables[thread_idx].empty()) {
            std::cout << "Same values detected " << std::endl;
        }
    }

    std::size_t before_deq_table_size = common_deq_acc_table.size();
    common_enq_acc_table.merge(common_deq_acc_table);
    if (common_deq_acc_table.size() != before_deq_table_size) {
        std::cout << "Some dequeued elements were never inserted" << std::endl;
    }
}
