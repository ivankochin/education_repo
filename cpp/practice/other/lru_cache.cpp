// LRU cache built on top of standard uset and self-writen double linked list
// with usage of C++20 heterogeneous find.

#include <memory>
#include <unordered_set>
#include <iostream>
#include <functional>

template<typename T>
class lru_cache {
    struct node {
        T data;
        node* next;
        node* prev;
    };
    struct node_hash {
        std::hash<T> hasher;
        std::size_t operator()(const node& node) const {
            return hasher(node.data);
        }
        std::size_t operator()(const T& key) const {
            return hasher(key);
        }
        using is_transparent = node_hash;
    };
    
    struct node_equal_to {
        bool operator()(const T& key, const node& node) const {
            return key == node.data;
        }
        bool operator()(const node& node, const T& key) const {
            return key == node.data;
        }
        bool operator()(const T& key1, const T& key2) const {
            return key1 == key2;
        }
        bool operator()(const node& node1, const node& node2) const {
            return node1.data == node2.data;
        }
        using is_transparent = node_equal_to;
    };
    std::unordered_set<node, node_hash, node_equal_to> uset;
    std::size_t max_size;
    node* first{nullptr};
    node* last{nullptr};
public:
    lru_cache(std::size_t size) : max_size(size) {}

    template <typename U>
    void refer(U&& u) {
        std::cout << "start, size is " << uset.size() << " el is " << u << std::endl;
        auto pos = uset.find(u);
        node* current_ptr;
        std::cout << "here0" << std::endl;
        if (pos == uset.end()) {
            if (uset.size() == max_size) {
                std::cout << "here last " << last->data << " " << last->prev->data << std::endl;
                if (last->prev) {
                    last->prev->next = nullptr;
                }
                auto prev = last->prev;
                uset.erase(*last);
                last = prev;
                std::cout << "here2 last " << last->data << " " << last->prev->data << std::endl;
            }

            auto [it, is_inserted] = uset.emplace(std::forward<U>(u), nullptr, nullptr);
            current_ptr = const_cast<node*>(&(*it));
        } else {
            current_ptr = const_cast<node*>(&(*pos));
            if (pos->next) current_ptr->next->prev = current_ptr->prev;
            if (pos->prev) current_ptr->prev->next = current_ptr->next;
            if (last == current_ptr) last = last->prev;
            std::cout << "here3" << std::endl;
        }
        if (first) {
            std::cout << "here4" << std::endl;
            current_ptr->next = first;
            std::cout << "here5" << std::endl;
            first->prev = current_ptr;
            std::cout << "here6" << std::endl;
        }
        
        std::cout << "here7" << std::endl;
        first = current_ptr;
        if (uset.size() == 1) {
            last = current_ptr;
        }
        if (uset.size() > 2)
        std::cout << "done last " << last->data << " " << last->prev->data << std::endl;
        std::cout << std::endl;
    }

    void display() const {
        node* ptr = first;
        while (ptr != last) {
            std::cout << ptr->data << std::endl;
            ptr = ptr->next;
        }
        std::cout << last->data << std::endl;
    }

};

int main() {
    lru_cache<int> cache(5);
    cache.refer(1);
    cache.refer(2);
    cache.refer(3);
    cache.refer(4);
    cache.refer(5);

    cache.refer(1);

    cache.refer(6);

    cache.display();
}
