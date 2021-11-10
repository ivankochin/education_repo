#include <cassert>
#include <utility>
#include <type_traits>

// TODO: implement allocator related stuff
// TODO: implement non-member functions
// TODO: implement std::atomic<my::shared_ptr>
// TODO: implement deduction guides
// TODO: implement weak/unique ptr

namespace my {

template <typename T>
class shared_ptr {
public:
    using element_type = std::remove_extent_t<T>;

private:

    template <typename Y>
    friend class shared_ptr;
    
    struct control_block_base {
        virtual element_type*& get_data() noexcept = 0;
        virtual long& get_ref_count() noexcept = 0;
        virtual ~control_block_base() {}
    };

    class control_block : public control_block_base {
        element_type* data;
        long ref_count;
    public:
        control_block(element_type* in_data, long in_ref_count)
         : data{in_data}
         , ref_count{in_ref_count}
        {}

        virtual element_type*& get_data() noexcept override {
            return data;
        }

        virtual long& get_ref_count() noexcept override {
            return ref_count;
        }

        virtual ~control_block() {
            delete data;
        }
    };

    template <typename Y, typename Deleter>
    class control_block_with_custom_deleter : public control_block_base {
        element_type* data;
        long ref_count;
        Deleter deleter;
    public:
        control_block_with_custom_deleter(Y* in_data, long in_ref_count, Deleter in_deleter)
         : data{in_data}
         , ref_count{in_ref_count}
         , deleter{std::move(in_deleter)}
        {}

        virtual element_type*& get_data() noexcept override {
            return data;
        }

        virtual long& get_ref_count() noexcept override {
            return ref_count;
        }

        ~control_block_with_custom_deleter() override {
            if(data != nullptr) {
                deleter(data);
            }
        }
    };

    template<typename ControlBlock>
    struct aliasing_control_block : public control_block_base {
        element_type* data;
        ControlBlock* real_block;
    public:
        aliasing_control_block(const ControlBlock& in_cb, element_type* in_data)
         : data{in_data}
         , real_block{const_cast<ControlBlock*>(&in_cb)} // TODO: think about nessesity of this const_cast
        {}

        virtual element_type*& get_data() noexcept override {
            return data;
        }

        long& get_ref_count() noexcept override {
            return real_block->get_ref_count();
        }

        virtual ~aliasing_control_block() override {
            delete real_block;
        }
    };
public:

    // Constructors
    constexpr shared_ptr() noexcept {}
    constexpr shared_ptr(std::nullptr_t) noexcept {}

    template <typename Y>
    explicit shared_ptr(Y* ptr)
     : cb{new control_block{ptr, 1}}
    {}

    template <typename Y, typename Deleter>
    explicit shared_ptr(Y* ptr, Deleter deleter)
     : cb{new control_block_with_custom_deleter<Y, Deleter>(ptr, 1, std::move(deleter))}
    {}

    template <typename Deleter>
    explicit shared_ptr(std::nullptr_t, Deleter deleter)
     : cb{new control_block_with_custom_deleter<element_type, Deleter>(nullptr, 1, std::move(deleter))}
    {}

    template<typename Y>
    shared_ptr(const shared_ptr<Y>& in_ptr, element_type* data) noexcept
     : cb{new aliasing_control_block<typename shared_ptr<Y>::control_block_base>(*in_ptr.cb, data)}
    {
        in_ptr.cb->get_ref_count()++;
    }

    shared_ptr(const shared_ptr& r) noexcept
     : cb{r.cb}
    {
        cb->get_ref_count()++;
    }

    shared_ptr(shared_ptr&& r) noexcept
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
        cb->get_ref_count()++;

        return *this;
    }

    shared_ptr& operator=(shared_ptr&& r) {
        if (cb && --cb->get_ref_count() == 0) {
            delete cb;
        }
        cb = nullptr;

        std::swap(cb, r.cb);

        return *this;
    }

    // Destructor
    ~shared_ptr() {
        if (cb && --cb->get_ref_count() == 0) {
            delete cb;
        }
    }

    // Methods
    element_type* get() const noexcept {
        return cb ? cb->get_data() : nullptr;
    }

    long use_count() const noexcept {
        return cb ? cb->get_ref_count() : 0;
    }

    void swap(shared_ptr& r) noexcept {
        control_block_base* tmp = r.cb;
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

    element_type& operator*() const noexcept {
        return *get();
    }

    element_type* operator->() const noexcept {
        return get();
    }

    template<typename Y = T>
    typename std::enable_if_t<std::is_array_v<Y>, element_type&> operator[](std::size_t index) {
        return *(get() + index);
    }

    explicit operator bool() const noexcept {
        return cb ? cb->get_data() != nullptr : false;
    }

private:
    control_block_base* cb{nullptr};
};

template<typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args) {
    return shared_ptr<T>{new T{std::forward<Args>(args)...}};
}

} // namespace my
