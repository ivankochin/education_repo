// clang++ -std=c++20 -Wall -Wextra -pthread hazard_ptr.cpp -o hazard_ptr

#include <atomic>
#include <memory>
#include <thread>
#include <string>
#include <iostream>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>
#include <array>
#include <functional>
#include <cassert>

// TODO:
// - Add a TLS bitmap to make it simpler to find the first free slot
// - If we exceed the number of slots, use the last slot to allocate a new chunk of 64 slots (with a bitmap attached to it)
//     - This chunk life time should be managed by a hazard pointer that is controlled by the thread that holds the initial chunk
//     - Since this array of atomic<void*> it should be fine to store with seq_cst there too
//     - Every time when we call retire, we walk the array until its end, if the last element of
//       the array is not nullptr -> we know it leads to a new chunk, so we create a hazard pointer to it,
//       protect it, and then go there to check this array too (recursively until we read all the arrays)
//     - When we protect a pointer, we also set a bit in the bitmap, if we see that the bitmap has all the
//       bits set except the last one - we create a new chunk.
//     - When we remove the protection, we cleanup a bit in the bitmap, if we see that the bitmap is empty
//       and that's not the first chunk - we call retire() for it, since all the dynamic chunks are protected
//       by hazard pointers.
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
  };

  namespace details {
    static constexpr std::size_t IN_USE_SLOTS = 64;
    static constexpr std::size_t GC_THRESHOLD = 100;

    // in_use uses atomics so the retiring thread can scan slots without locks.
    // to_retire is only ever accessed by the owning thread, so no atomics needed.
    struct tls_data {
        tls_data() {
            for (auto& slot : in_use)
                slot.store(nullptr, std::memory_order_relaxed);
        }
        std::array<std::atomic<void*>, IN_USE_SLOTS> in_use;
        std::vector<std::pair<void*, std::function<void()>>> to_retire;
    };

    class hp_manager {
        // unique_ptr avoids moving tls_data (std::atomic is non-movable) on map rehash.
        using global_hash_table_t = std::unordered_map<std::thread::id, std::unique_ptr<tls_data>>;

        global_hash_table_t threads_hash{};
        std::shared_mutex rw_lock{};

        tls_data& get_tls_data() {
            thread_local tls_data* cache = nullptr;
            if (cache) return *cache;

            std::unique_lock write_lock(rw_lock);
            auto& ptr = threads_hash[std::this_thread::get_id()];
            if (!ptr) ptr = std::make_unique<tls_data>();
            cache = ptr.get();
            return *cache;
        }

        // Called under shared_lock so threads_hash is stable while we iterate.
        bool is_in_use(void* ptr) {
            for (auto& [tid, tls] : threads_hash) {
                for (auto& slot : tls->in_use) {
                    if (slot.load(std::memory_order_acquire) == ptr) return true;
                }
            }
            return false;
        }

    public:
        void retire(void* ptr, std::function<void()> cleanup) {
            auto& tls = get_tls_data();
            tls.to_retire.emplace_back(ptr, std::move(cleanup));

            if (tls.to_retire.size() >= GC_THRESHOLD) {
                std::shared_lock read_lock(rw_lock);

                std::vector<std::pair<void*, std::function<void()>>> new_to_retire;
                for (auto& [p, del] : tls.to_retire) {
                    if (is_in_use(p))
                        new_to_retire.emplace_back(p, std::move(del));
                    else
                        del();
                }
                tls.to_retire = std::move(new_to_retire);
            }
        }

        void reset_protection(int64_t& cell_idx, const void* new_value) {
            auto& tls = get_tls_data();
            auto value = const_cast<void*>(new_value);

            if (cell_idx >= 0) {
                // ??? Understand why do we need seq_cst here? Why not release?
                tls.in_use[cell_idx].store(value, std::memory_order_seq_cst);
                if (new_value == nullptr)
                    cell_idx = -1;  // release the slot so others can claim it
            } else if (new_value != nullptr) {
                for (int64_t i = 0; i < static_cast<int64_t>(IN_USE_SLOTS); i++) {
                    if (tls.in_use[i].load(std::memory_order_relaxed) == nullptr) {
                        tls.in_use[i].store(value, std::memory_order_seq_cst);
                        cell_idx = i;
                        return;
                    }
                }
                assert(false && "All IN_USE_SLOTS slots occupied: too many simultaneous hazard_pointers per thread");
            }
            // cell_idx < 0 && new_value == nullptr: nothing to do.
        }
    };

    static hp_manager& instance() {
        static hp_manager singleton;
        return singleton;
    }

    void reset_protection(int64_t& cell_idx, const void* new_value) {
        instance().reset_protection(cell_idx, new_value);
    }

    template <typename T, typename D>
    void retire(T* target, D deleter) {
        instance().retire(target, [target, deleter = std::move(deleter)]() {
            deleter(target);
        });
    }
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
        return cell_idx < 0;
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

    void swap(hazard_pointer& other) noexcept {
        std::swap(cell_idx, other.cell_idx);
    }

  private:
    int64_t cell_idx = -1;
  };

  hazard_pointer make_hazard_pointer() {
    return hazard_pointer();
  }
  void swap(hazard_pointer& a, hazard_pointer& b) noexcept { a.swap(b); }

  template<class T, class D>
  void hazard_pointer_obj_base<T, D>::retire(D deleter) noexcept {
    details::retire(static_cast<T*>(this), std::move(deleter));
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
