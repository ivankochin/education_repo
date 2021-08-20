#include <iostream>
#include <iterator>
#include <algorithm>
#include <vector>
#include <utility>
#include <thread>
#include <mutex>
#include <functional>
#include <queue>
#include <atomic>

// build: g++ merge_sort.cpp -g -O0 -pthread -o merge_sort.exe

//TODO:
// - improve local waiters interface
// - implement condition variable optimization
// - relax memory fences where possible
// - implement testing on bigger data + operation time measurements

namespace my {
namespace details {

template<typename It>
using value_type = typename std::iterator_traits<It>::value_type;

template<typename It>
using copy_vector = std::vector<value_type<It>>;

template<typename InputIt, typename OutputIt, typename Comparator>
void merge_sorted(InputIt l_begin, InputIt l_end, InputIt r_begin, InputIt r_end, OutputIt out, Comparator comp) {
    while(l_begin != l_end || r_begin != r_end) {
        if (r_begin == r_end || (l_begin != l_end && comp(*l_begin, *r_begin))) {
            *out = *l_begin++;
        } else {
            *out = *r_begin++;
        }
        ++out;
    }
}

template<typename RAIt, typename CopyIter, typename Comparator>
void sequential_merge_sort_impl(
    RAIt begin, RAIt end,
    CopyIter copy_begin, CopyIter copy_end,
    Comparator comp
) {
    auto distance = std::distance(begin, end);

    if (distance > 1) {
        auto half = begin + distance / 2;
        auto copy_half = copy_begin + distance / 2;
        sequential_merge_sort_impl(begin, half, copy_begin, copy_half, comp);
        sequential_merge_sort_impl(half, end, copy_half, copy_end, comp);

        merge_sorted(begin, half, half, end, copy_begin, comp);
        std::copy(copy_begin, copy_end, begin);
    }
}

class thread_pool {
    using task_type = std::function<void()>;

    void try_execute_task() {
        task_type task{nullptr};
        {
            std::lock_guard<std::mutex> lc(my_queue_mutex);
            if (!my_task_queue.empty()) {
                task = std::move(my_task_queue.front());
                my_task_queue.pop();
            }
        }
        if (task) {
            task();
            --my_tasks_count;
        } else {
            std::this_thread::yield();
        }
    }

    thread_pool()
        : my_thread_pool(std::thread::hardware_concurrency() - 1) // reserve 1 thread as an external
        , my_task_queue{}
        , my_queue_mutex{}
        , my_tasks_count{0}
        , my_continue_work_flag{true}
    {
        auto thread_body = [this] {
            while(my_continue_work_flag) {
                try_execute_task();
            }
        };
        for (auto& thread : my_thread_pool) {
            thread = std::thread(thread_body);
        }
    }

    ~thread_pool() {
        my_continue_work_flag.store(false);
        for (auto& thread : my_thread_pool) {
            thread.join();
        }
    }

    static thread_pool& instance() {
        static thread_pool static_thread_pool;
        return static_thread_pool;
    }

public:
    template<typename Task>
    static void spawn(Task&& t) {
        auto& tp = instance();
        ++tp.my_tasks_count;
        std::lock_guard<std::mutex> lc(tp.my_queue_mutex);
        tp.my_task_queue.push(std::forward<Task>(t));
    }

    static void try_execute() {
        auto& tp = instance();
        if (tp.my_tasks_count > 0) {
            tp.try_execute_task();
        }
    }

private:
    std::vector<std::thread> my_thread_pool;
    std::queue<task_type> my_task_queue;
    std::mutex my_queue_mutex;
    std::atomic<std::size_t> my_tasks_count;
    std::atomic<bool> my_continue_work_flag;
};

template<typename RAIt, typename CopyIter, typename Comparator>
void parallel_merge_sort_impl(
    RAIt begin, RAIt end,
    CopyIter copy_begin, CopyIter copy_end,
    Comparator comp
) {
    auto distance = std::distance(begin, end);

    if (distance > 1) {
        auto half = begin + distance / 2;
        auto copy_half = copy_begin + distance / 2;
        std::atomic<std::size_t> local_counter{2};
        thread_pool::spawn([&] {parallel_merge_sort_impl(begin, half, copy_begin, copy_half, comp); local_counter--;});
        thread_pool::spawn([&] {parallel_merge_sort_impl(half, end, copy_half, copy_end, comp); local_counter--;});
        while(local_counter) {
            thread_pool::try_execute();
        }

        merge_sorted(begin, half, half, end, copy_begin, comp);
        std::copy(copy_begin, copy_end, begin);
    }
}

} // namespace details

struct par{};
struct seq{};

template<typename RAIt, typename Comparator>
void merge_sort(RAIt begin, RAIt end, Comparator comp) {
    using value_type = typename std::iterator_traits<RAIt>::value_type;
    std::vector<value_type> copy_vec(std::distance(begin, end));
    details::sequential_merge_sort_impl(begin, end, std::begin(copy_vec), std::end(copy_vec), comp);
}

template<typename RAIt>
void merge_sort(RAIt begin, RAIt end) {
    using value_type = typename std::iterator_traits<RAIt>::value_type;
    merge_sort(begin, end, std::less<value_type>{});
}

template<typename Policy, typename RAIt, typename Comparator>
void merge_sort(Policy, RAIt begin, RAIt end, Comparator comp) {
    using value_type = typename std::iterator_traits<RAIt>::value_type;
    std::vector<value_type> copy_vec(std::distance(begin, end));
    details::sequential_merge_sort_impl(begin, end, std::begin(copy_vec), std::end(copy_vec), comp);
}

template<typename RAIt, typename Comparator>
void merge_sort(par, RAIt begin, RAIt end, Comparator comp) {
    using value_type = typename std::iterator_traits<RAIt>::value_type;
    std::vector<value_type> copy_vec(std::distance(begin, end));
    details::parallel_merge_sort_impl(begin, end, std::begin(copy_vec), std::end(copy_vec), comp);
}

template<typename Policy, typename RAIt>
void merge_sort(Policy policy, RAIt begin, RAIt end) {
    using value_type = typename std::iterator_traits<RAIt>::value_type;
    merge_sort(policy, begin, end, std::less<value_type>{});
}

} // namespace my

int main() {
    std::vector<int> vec{2, 5, 2, 1, 9, 7};
    my::merge_sort(std::begin(vec), std::end(vec));

    for(auto el: vec) {
        std::cout << el << " ";
    }
    std::cout << std::endl;

    std::vector<int> vec1{2, 5, 2, 1, 9, 7};
    my::merge_sort(my::par{}, std::begin(vec1), std::end(vec1));
    for(auto el: vec1) {
        std::cout << el << " ";
    }
    std::cout << std::endl;
}
