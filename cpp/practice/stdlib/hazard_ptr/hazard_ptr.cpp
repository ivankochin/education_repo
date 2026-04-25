#include <atomic>
#include <memory>
#include <thread>
#include <vector>
#include <string>
#include <iostream>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>

// TODO:
// - Complete retire
//    - add the internal call to the public API one
//    - add cast to the internal call to the right type + call the deleter
// - Idea of the better implementation (without having a race that this implementation has)
//     - Change tls_data
//          - to_retire can be a global hash map under a mutex - we can move it out of retire
//          - in_use -> change it to be a static array of let's say 256 elements (configurable)
//                    - if we fill all the 256 element, we store in the last element pointer to the next 256 el array
//                    - I should think better how to reuse existing elements or understand that an extra array allocated can be cleaned up (it could be a second step)
//                    - we can even make chunks smaller (64 elements) so we can store a bitmap to check what entries has filled and what hasn't
//                    - if the thread that calls retire() finds that the bitmap is full it knows that it should be able to cast the last element to an array pointer
//                            - think what's the synchronisation in this case

namespace my {

  template<class T, class D = std::default_delete<T>>
  class hazard_pointer_obj_base {
  public:
    void retire(D deleter = D()) noexcept;
  protected:
    hazard_pointer_obj_base() = default;
    hazard_pointer_obj_base(const hazard_pointer_obj_base&) = default;
    hazard_pointer_obj_base(hazard_pointer_obj_base&&) = default;
    hazard_pointer_obj_base& operator=(const hazard_pointer_obj_base&) = default;
    hazard_pointer_obj_base& operator=(hazard_pointer_obj_base&&) = default;
    ~hazard_pointer_obj_base() = default;
  private:
  };

  namespace details {
    // DIFFERENT IDEAS:
    // Global vectors + RW lock ???
    // Thread safe hash map + global epoch count ???
    // Try lists instead of vector (How bad would it be) ???
    // Try flat lists ???

    // Thread local vectors + RW lock

    // Hash map of thread local vectors + RW lock
    // ? Requires that protection epoch should start and end from one thread
    // IT CAN'T WORK WITH VECTORS SINCE THEY CAN BE REALLLOCATED TO EXTEND THEIR SIDE AND TO_RETIRE WILL SEE THE DELETED MEMORY!!!
#define RW_LOCK_HASH_MAP_OF_VECTORS
#if defined(RW_LOCK_HASH_MAP_OF_VECTORS)
    class hp_manager {
        struct tls_data {
            std::vector<const void *> in_use;
            std::vector<const void *> to_retire;
        };
        using global_hash_table_t = std::unordered_map<std::thread::id, tls_data>;

        global_hash_table_t threads_hash{};
        std::shared_mutex rw_lock{};
        std::atomic<std::size_t> to_retire_count{0};

        static constexpr std::atomic<std::size_t> gc_threshold = 100;

        // TODO: Think about removing unused vectors !!! // It should be fine to remove our own data without a conflict
        tls_data& get_tls_data() {
            auto tid = std::this_thread::get_id();
            global_hash_table_t::iterator tls_it;

            { // Check the TLS existence with the read lock
                std::shared_lock read_lock(rw_lock);
                tls_it = threads_hash.find(tid);
                if (tls_it != std::end(threads_hash))
                    return tls_it->second;
            }

            std::unique_lock write_lock(rw_lock);
            return threads_hash[tid];
        }

        bool is_in_use(const void* ptr) {
            for (auto& [tid, tls]: threads_hash) {
                for (auto inuse_it = std::begin(tls.in_use); inuse_it != std::end(tls.in_use); ++inuse_it) {
                    if (*inuse_it == ptr) return true;
                }
            }

            return false;
        }
    public:
        void retire(const void *ptr) {
            auto& [in_use, to_retire] = get_tls_data();

            to_retire.emplace_back(ptr);
            // TODO: Can I use release or relaxed here?
            auto old_val = to_retire_count.fetch_add(1);
            // We do cleanup in case of equality only to avoid blocking many threads since the
            // job of garbage collection is always done by only one of them. The current approach
            // guarantees that only one thread waits on write lock.
            if (old_val == gc_threshold) {
                std::unique_lock write_lock(rw_lock);

                // iterate over all to_retire and check which ones could be cleaned up
                for (auto& [tid, tls]: threads_hash) {
                    std::vector<const void *> new_to_retire;
                    for (auto retire_it = std::begin(tls.to_retire); retire_it != std::end(tls.to_retire); ++retire_it) {
                        for (auto& [tid, tls]: threads_hash) {
                            if (is_in_use(*retire_it)) {
                                new_to_retire.emplace_back(*retire_it);
                            } else {
                                // Call deleter
                            }
                        }
                    }
                    tls.to_retire = new_to_retire;
                }
            }
        }

        void reset_protection(int64_t& cell_idx, const void* new_value) {
            auto& [in_use, to_retire] = get_tls_data();

            if (cell_idx > 0) {
                in_use[cell_idx] = new_value;
            } else {
                cell_idx = in_use.size();
                in_use.push_back(new_value);
            }
        }
    };

    static hp_manager& instance() {
        static hp_manager singleton;
        return singleton;
    }

    void reset_protection(int64_t cell_idx, const void* new_value) {
        instance().reset_protection(cell_idx, new_value);
    }

    template <typename T>
    void retire(const hazard_pointer_obj_base<T>& target) {
        instance().retire(target);
    }
#endif // RW_LOCK_HASH_MAP_OF_VECTORS
  } // namespace details


  class hazard_pointer {
  public:
    hazard_pointer() noexcept = default;
    hazard_pointer(hazard_pointer&&) noexcept = default;
    hazard_pointer& operator=(hazard_pointer&&) noexcept = default;
    ~hazard_pointer() {
        reset_protection(nullptr);
    }

    bool empty() const noexcept {
        return cell_idx >= 0;
    }

    template<class T> T* protect(const std::atomic<T*>& src) noexcept {
        T* ptr = src.load(std::memory_order::relaxed);
        while (!try_protect(ptr, src)) {}
        return ptr;
    }

    template<class T> bool try_protect(T*& ptr, const std::atomic<T*>& src) noexcept {
        T* old = ptr;
        reset_protection(old);
        ptr = src.load(std::memory_order_acquire);
        return ptr == old;
    }

    template<class T> void reset_protection(const T* ptr) noexcept {
        details::reset_protection(cell_idx, ptr);
    }

    void reset_protection(nullptr_t ptr = nullptr) noexcept {
        details::reset_protection(cell_idx, ptr);
    }

    void swap(hazard_pointer&) noexcept;
private:
    int64_t cell_idx = -1;
  };

  hazard_pointer make_hazard_pointer() {
    return hazard_pointer();
  }
  void swap(hazard_pointer&, hazard_pointer&) noexcept {}

  template<class T, class D>
  void hazard_pointer_obj_base<T, D>::retire(D deleter) noexcept {
    // implementation here
  }
} // namespace my

struct Name : public my::hazard_pointer_obj_base<Name> {
  Name (const char *in)
    : str(in)
  {};

  std::string str;
};

std::vector<Name> names = {"Vanya", "Sasha", "Yulya", "Andrey"};
std::atomic<Name*> name;

// called often and in parallel!
void print_name() {
  my::hazard_pointer h = my::make_hazard_pointer();
  Name* ptr = h.protect(name); // Protection epoch starts
  // ... safe to access *ptr

  std::cout << "thread " << std::this_thread::get_id() << " name " << ptr->str << std::endl;
} // Protection epoch ends.

// called rarely, but possibly concurrently with print_name
void update_name() {
    static std::atomic<int> pos = 0;

    int old_pos, next_pos;
    do {
        old_pos = pos.load(std::memory_order_relaxed);
        next_pos = (old_pos + 1) % names.size();
    } while (!pos.compare_exchange_weak(old_pos, next_pos, std::memory_order_relaxed));

    Name* new_name = new Name(names[next_pos]);
    Name* ptr = name.exchange(new_name);
    ptr->retire();
}

std::size_t num_epochs = 1000000;
std::size_t write_epoch = 50000;
void thread_func() {
    for (std::size_t epoch = 0; epoch < num_epochs; ++epoch) {
        if (epoch % write_epoch) {
            print_name();
        } else {
            update_name();
        }
    }
}

int main() {
    name = new Name(names[0]);
    std::vector<std::jthread> threads(2);
    for (auto& thread: threads) {
        thread = std::jthread(thread_func);
    }
}
