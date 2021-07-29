#include <iostream>
#include <iterator>
#include <algorithm>
#include <vector>
#include <utility>

//TODO:
// - implement overload without comparator
// - implement parallel
// - implement politics (via template argument)
// - implement testin on bigger data + operation time measurements

namespace my {

template<typename It>
using value_type = typename std::iterator_traits<It>::value_type;

template<typename It>
using copy_vector = std::vector<value_type<It>>;

template<typename InputIt, typename OutputIt, typename Comparator>
void merge_sorted(InputIt l_begin, InputIt l_end, InputIt r_begin, InputIt r_end, OutputIt out, Comparator comp) {
    while(l_begin != l_end || r_begin != r_end) {
        if (r_begin == r_end || (l_begin != l_end && comp(*l_begin, *r_begin))) {
            *out = *l_begin++;
        } else {
            *out = *r_begin++;
        }
        ++out;
    }
}

template<typename RAIt, typename CopyIter, typename Comparator>
void sequential_merge_sort_impl(
    RAIt begin, RAIt end,
    CopyIter copy_begin, CopyIter copy_end,
    Comparator comp
) {
    auto distance = std::distance(begin, end);

    if (distance > 1) {
        auto half = begin + distance / 2;
        auto copy_half = copy_begin + distance / 2;
        sequential_merge_sort_impl(begin, half, copy_begin, copy_half, comp);
        sequential_merge_sort_impl(half, end, copy_half, copy_end, comp);

        merge_sorted(begin, half, half, end, copy_begin, comp);
        std::copy(copy_begin, copy_end, begin);
    }
}

template<typename RAIt, typename Comparator /* = std::less<value_type<RAIt>*/>
void merge_sort(RAIt begin, RAIt end, Comparator comp = Comparator{}) {
    using value_type = typename std::iterator_traits<RAIt>::value_type;
    std::vector<value_type> copy_vec(std::distance(begin, end));
    sequential_merge_sort_impl(begin, end, std::begin(copy_vec), std::end(copy_vec), comp);
}

} // namespace my

int main() {
    std::vector<int> vec{2, 5, 2, 1, 9, 7};
    my::merge_sort(std::begin(vec), std::end(vec), std::less<int>{});

    for(auto el: vec) {
        std::cout << el << " ";
    }
    std::cout << std::endl;
}
