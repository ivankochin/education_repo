#include <vector>
#include <iostream>

/* TODO:
 - implement remove max
 - implement sort using heap
*/

template <typename T, typename Comp = std::less<T>>
class binary_heap {
    std::vector<T> data;
    Comp comp;
    using data_iterator = typename std::vector<T>::iterator;

    data_iterator get_parent(std::size_t pos) {
        auto parent_pos = pos;
        parent_pos -= (pos % 2) ? 1 : 2;
        parent_pos /= 2;
        return std::begin(data) + parent_pos;
    }
public:
    template<typename Y>
    void insert(Y&& new_el) {
        data.emplace_back(std::forward<Y>(new_el));

        if (std::size(data) == 1) return;

        std::size_t pos = std::size(data) - 1;
        data_iterator current_it, parent_it;
        while (pos > 0) {
            auto current_it = std::begin(data) + pos;
            auto parent_it = get_parent(pos);

            if (comp(*current_it, *parent_it)) {
                break;
            }
            std::iter_swap(current_it, parent_it);
            pos = std::distance(std::begin(data), parent_it);
        }
    }

    T get_max() const {
        return data.front();
    };

    void print() const {
        for (const auto& el: data) {
            std::cout << el << " ";
        }
        std::cout << std::endl;
    }
};
/*
            0
           / \
          /   \
         /     \
        1       2
       / \     / \
      /   \   /   \
     3     4 5     6
   / |   / | | \   | \
  7  8  9  1011 12 13 14
*/

int main() {
    binary_heap<int> bh;
    bh.insert(1);
    bh.insert(2);
    bh.insert(5);
    bh.insert(3);
    bh.insert(7);

    bh.print();

    std::cout << "max is: " << bh.get_max() << std::endl;
}