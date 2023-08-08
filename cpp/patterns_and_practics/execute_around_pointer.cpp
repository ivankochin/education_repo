// https://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Execute-Around_Pointer

#include <iostream>

struct B {
    void func() {
        std::cout << "B func" << std::endl;
    }
};

template <typename T>
struct A {
    struct proxy {
        T* operator->() {
            std::cout << "proxy ->" << std::endl;
            return &t;
        }

        void func() {
            std::cout << "proxy func" << std::endl;
        }

        T t;
    };

    proxy operator->() {
        std::cout << "A ->" << std::endl;
        return proxy{};
    }
};

int main() {
    A<B> a;

    // proxy::operator-> operator implicitly calls on each A::operator-> call 
    //   and can contain additional logic (e.g. thread safety).
    // There is no way to access proxy func.
    a->func();
}
