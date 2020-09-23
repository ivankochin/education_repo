#include <iostream>

template <int... Is>
struct index_sequence {};

namespace {

template <int I, int... Ints>
struct make_index_sequence_impl{
    using type = typename make_index_sequence_impl<I - 1, I, Ints...>::type;
};

template <int... Ints>
struct make_index_sequence_impl<0, Ints...> {
    using type  = index_sequence<0, Ints...>;
};

}

template <size_t N>
using make_index_sequence = typename make_index_sequence_impl<N>::type;


template <int... Seq>
void external_function_impl(index_sequence<Seq...>) {
    (std::cout << Seq << ", ", ...); // requires c++17
}

template<size_t N, typename Seq = make_index_sequence<N>>
void external_function() {
    external_function_impl(Seq());
}

int main() {
    external_function<10>();
}
