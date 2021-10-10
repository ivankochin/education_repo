#include <utility>
#include <atomic>

template <typename T>
struct lock_free_stack {
private:
    struct node {
        T data;
        node* next;
    };

    std:atomic<node*> head{nullptr};

    lock_free_stack(const lock_free_stack& other) = delete;
    lock_free_stack& operator=(const lock_free_stack& other) = delete;

    void push(T elem) {
        node* new_head = new node{std::move(elem), head.load()};
        while(!head.compare_exchange_weak(new_head->next, new_head)) {}
    }

    T pop(); // TODO: implement
};