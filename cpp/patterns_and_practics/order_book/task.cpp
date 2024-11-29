// first = {1,2,3,4,5}
// second = {1,2,6}
// output = {1,1,2,2,3,4,5,6}

namespace mystd {
template <typename FirstIt, typename SecondIt, typename OutputIt>
OutputIt merge(FirstIt begin1, FirstIt end1, SecondIt begin2, SecondIt end2, OutputIt output) {
    for (;begin1 != end1 && begin2 != end2; ) {
        ++output;
        if (*begin1 < *begin2) {
            *output = *begin1;
            ++begin1;
        } else {
            *output = *begin2;
            ++begin2;
        }
    }
    std::copy(begin1, end1, output);
    std::copy(begin2, end2, output);
    return output;
}

template <typename Container>
struct back_inserter {
    back_inserter(Constainer& cont) : m_cont(cont) {}
    Container::value& operator*() {
        m_cont.emplace_back(Container::value{});
        return m_cont.back();
    }

    

    Container& m_cont;
};
}


int main() {
    std::vector<int> a = {1, 2, 3};
    std::list<int> b = {1, 2, 3};
    std::vector<int> c;

    merge(std::begin(a), std::end(a), std::begin(b), std::end(b), std::back_inserter(c));
}