#include <cassert>
#include "shared_ptr.hpp"

void test_common_functionality() {
    struct Object {};

    my::shared_ptr<int> empty_ptr{};
    assert(0 == empty_ptr.use_count());
    assert(nullptr == empty_ptr.get());
    assert(!empty_ptr);

    Object* obj_ptr = new Object;
    my::shared_ptr<Object> ptr1{obj_ptr};
    assert(1 == ptr1.use_count());
    assert(obj_ptr == ptr1.get());
    assert(obj_ptr);

    my::shared_ptr<Object> ptr2 = ptr1;
    assert(2 == ptr2.use_count());
    assert(obj_ptr == ptr2.get());
    assert(obj_ptr);

    my::shared_ptr<Object> ptr3 = std::move(ptr1);
    assert(2 == ptr3.use_count());
    assert(obj_ptr == ptr3.get());
}

void test_custom_deleter() {
    struct Object {};

    std::size_t deleter_call_count{0};
    {
        my::shared_ptr<int> empty_ptr{nullptr, [&](int* ptr) {
            ++deleter_call_count;
            delete ptr;
        }};
        assert(1 == empty_ptr.use_count());
        assert(nullptr == empty_ptr.get());
    }
    assert(deleter_call_count == 0);

    {
        Object* obj_ptr = new Object;
        my::shared_ptr<Object> ptr1{obj_ptr, [&](Object* ptr) {
            ++deleter_call_count;
            delete ptr;
        }};
        assert(1 == ptr1.use_count());
        assert(obj_ptr == ptr1.get());
        
        my::shared_ptr<Object> ptr2 = ptr1;
        assert(2 == ptr2.use_count());
        assert(obj_ptr == ptr2.get());

        my::shared_ptr<Object> ptr3 = std::move(ptr1);
        assert(2 == ptr3.use_count());
        assert(obj_ptr == ptr3.get());
    }
    assert(deleter_call_count = 1);
}

void test_aliasing_ctor() {
    struct Field {};
    struct Object {
        Field f;
    };

    my::shared_ptr<Field> aliasing_ptr{nullptr};
    Object* obj_ptr = new Object;
    Field* field_ptr = &obj_ptr->f;
    {
        my::shared_ptr<Object> ptr1{obj_ptr};
        aliasing_ptr = my::shared_ptr<Field>(ptr1, &ptr1->f);
    }

    assert(aliasing_ptr.get() == field_ptr);
}

int main() {
    test_common_functionality();
    test_custom_deleter();
    test_aliasing_ctor();
}