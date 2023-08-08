// https://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Execute-Around_Pointer

#include <iostream>

struct B {
    void func() {
        std::cout << "B func" << std::endl;
    }
};

template <typename T>
struct execute_around {
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
        std::cout << "execute_around ->" << std::endl;
        return proxy{};
    }
};

int main() {
    execute_around<B> b;

    // proxy::operator-> operator implicitly calls on each A::operator-> call 
    //   and can contain additional logic (e.g. thread safety).
    // Note that in case of thread safety additional logic (e.g. locking the mutes on each proxy::operator->) call
    //   you should be very careful because:
    //     1: Locking becames implicit
    //     2: You should access the wrapped class only through -> (taking begin() from wrapped vector and then using * operator is an error)
    // There is no way to access proxy func.
    b->func();
}
