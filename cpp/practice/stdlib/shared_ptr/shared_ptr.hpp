#include <utility>
#include <type_traits>

// TODO: implement other methods
// TODO: implement std::atomic<std::shared_ptr>

namespace my {

template <typename T>
class shared_ptr {
    struct control_block {
        T* data{nullptr};
        long ref_count{0};

        ~control_block()  {
            delete data;
        }
    };

    template <typename Deleter>
    struct control_block_with_custom_deleter : public control_block {
        Deleter deleter;

        ~control_block_with_custom_deleter() override {
            delete control_block::data;
        }
    };
public:
    using element_type = std::remove_extent_t<T>;

    // Constructors
    constexpr shared_ptr() noexcept
     : cb{nullptr}
    {}

    constexpr shared_ptr(std::nullptr_t) noexcept
     : cb{nullptr}
    {}

    template <typename Y>
    explicit shared_ptr(Y* ptr)
     : cb{new control_block{ptr, 1}}
    {}

    template <typename Y, class Deleter>
    explicit shared_ptr(Y* ptr, Deleter deleter)
     : cb{new control_block_with_custom_deleter{ptr, 1, std::move(deleter)}}
    {}

    template <class Deleter>
    explicit shared_ptr(std::nullptr_t, Deleter deleter)
     : cb{new control_block_with_custom_deleter{nullptr, 1, std::move(deleter)}}
    {}

    shared_ptr(const shared_ptr& r) noexcept
     : cb{r.cb}
    {
        cb->ref_count++;
    }

    shared_ptr(shared_ptr&& r) noexcept
     : cb{nullptr}
    {
        std::swap(cb, r.cb);
    }

    // Assignments
    shared_ptr& operator=(const shared_ptr& r) {
        if (cb && --cb->ref_count == 0) {
            delete cb;
            cb = nullptr;
        }
        cb = r.cb;
        cb->ref_count++;

        return *this;
    }

    shared_ptr& operator=(shared_ptr&& r) {
        if (cb && --cb->ref_count == 0) {
            delete cb;
        }
        cb = nullptr;

        std::swap(cb, r.cb);

        return *this;
    }


    // Destructor
    ~shared_ptr() {
        if (cb && --cb->ref_count == 0) {
            delete cb;
        }
    }

    // Methods
    element_type* get() const noexcept {
        return cb ? cb->data : nullptr;
    }

    long use_count() const noexcept {
        return cb ? cb->ref_count : 0;
    }

    void swap(shared_ptr& r) noexcept {
        control_block* tmp = r.cb;
        r.cb = cb;
        cb = tmp;
    }

    void reset() noexcept {
        shared_ptr{}.swap(*this);
    };

    template <typename Y>
    void reset(Y* ptr) {
        shared_ptr{ptr}.swap(*this);
    }

    template <typename Y, class Deleter>
    void reset(Y* ptr, Deleter deleter) {
        shared_ptr{ptr, std::move(deleter)}.swap(*this);
    }

    T& operator*() const noexcept {
        return *get();
    }

    T* operator->() const noexcept {
        return get();
    }

    explicit operator bool() const noexcept {
        return cb ? cb->data == nullptr : false;
    }

private:
    control_block* cb;
};

} // namespace my