#include <iostream>

// Must be constructed from two arguments
// (second may be binded to some value)
struct widget {
    widget(double first, int second) { std::cout << first << " : " << second << std::endl; }
};

namespace {

// Allows to create function overload with fictitious argument
// without argument instance creation
template <typename T>
struct type_2_type {
    using type = T;
};

template <typename T, typename U>
T* create_impl(const U& arg, type_2_type<T>) {
    std::cout<<"usual creation"<<std::endl;
    return new T(arg);
}

template <typename U>
widget* create_impl(const U& arg, type_2_type<widget>) {
    std::cout<<"widget creation"<<std::endl;
    return new widget(arg, 1);
}

} // internal namespace

template <typename T, typename U>
T* create (const U& arg) {
    return create_impl(arg, type_2_type<T>());
}

int main() {
    auto int_instance = create<int>(1);
    auto widget_instance = create<widget>(3.);
}
