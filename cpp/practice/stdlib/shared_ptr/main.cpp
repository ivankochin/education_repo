#include <cassert>
#include "shared_ptr.hpp"

struct Object {};

int main() {
    my::shared_ptr<int> empty_ptr{};
    assert(0 == empty_ptr.use_count());
    assert(nullptr == empty_ptr.get());

    Object* obj_ptr = new Object;
    my::shared_ptr<Object> ptr1{obj_ptr};
    assert(1 == ptr1.use_count());
    assert(obj_ptr == ptr1.get());

    my::shared_ptr<Object> ptr2 = ptr1;
    assert(2 == ptr2.use_count());
    assert(obj_ptr == ptr2.get());

    
    my::shared_ptr<Object> ptr3 = std::move(ptr1);
    assert(2 == ptr3.use_count());
    assert(obj_ptr == ptr3.get());
}
