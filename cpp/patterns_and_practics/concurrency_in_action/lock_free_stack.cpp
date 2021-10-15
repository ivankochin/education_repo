#include <utility>
#include <atomic>
#include <memory>
#include <iostream>

// TODO: relax the fences where possible
// TODO: Implement the version with two reference counters (internal/external)

template <typename T>
struct lock_free_stack {
private:
    struct node {
        T data;
        std::shared_ptr<node> next;
    };

    // Requires C++20
    std::atomic<std::shared_ptr<node>> head{};

public:
    lock_free_stack() = default;
    lock_free_stack(const lock_free_stack& other) = delete;
    lock_free_stack& operator=(const lock_free_stack& other) = delete;

    void push(T elem) {
        std::shared_ptr<node> new_head = std::make_shared<node>(std::move(elem), head.load());
        while(!head.compare_exchange_weak(new_head->next, new_head)) {}
    }

    T pop() {
        std::shared_ptr<node> old_head = head.load();
        while(old_head.get() && !head.compare_exchange_weak(old_head, old_head->next)) {}

        return old_head->data;
    }
};

int main() {
    lock_free_stack<int> int_stack;
    int_stack.push(1);

    std::cout << int_stack.pop() << std::endl;
}
