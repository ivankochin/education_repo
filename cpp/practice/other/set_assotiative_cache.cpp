#include <vector>
#include <array>
#include <iostream>
#include <cassert>
#include <cstdlib>

template <std::size_t LinesNum = 256, std::size_t LinesInSet = 4>
class set_assotiative_cache {
    using cache_line_type = std::uint64_t;
    constexpr static std::size_t set_num = LinesNum / LinesInSet; // 64 for default configuration

    // Example for default configuration:
    // Offest - 6 bits (since uint64_t contains 64 bits = 2^6)
    // Index  - 6 bits (since set_num = 64 = 2^6)
    // Tag    - 52 bits (64 bits in address - 6 Index bits - 6 Offset bits)
    // ---------------------------------------------------------------------
    // |                  Tag (52)                  | Index (6) | Offset (6)|
    // ---------------------------------------------------------------------
    struct cache_line {
        uint64_t        tag;
        cache_line_type value;
    };

    std::array<cache_line, LinesNum> cache{};

public:
    void push_line(uint64_t addr, cache_line_type value) {
        assert(addr % (sizeof(cache_line_type) * 8) == 0);
        addr >>= 6; // Discard offset
        
        // Obtain index
        std::size_t set_start_idx = (addr % set_num) * LinesInSet;
        addr /= set_num;

        // Remain addr is tag, need to add 1 since empty tag is 0
        std::size_t tag = addr + 1;

        for (std::size_t set_el = 0;  set_el < LinesInSet; ++set_el) {
            cache_line &line = cache[set_start_idx + set_el];
            if (line.tag == 0) {
                // Place element on the empty spot
                line.tag   = tag;
                line.value = value;
                return;
            } else if (line.tag == tag) {
                // Renew current tag value
                line.value = value;
                return;
            }
        }

        // Replace random element
        cache_line &line_to_replace = cache[set_start_idx + (std::rand() % LinesInSet)];
        line_to_replace.tag   = addr;
        line_to_replace.value = value;
    }

    std::pair<bool, uint8_t> find(uint64_t addr) {
        std::size_t offset = addr % (sizeof(cache_line_type) * 8);
        addr /= (sizeof(cache_line_type) * 8);

        // Obtain index
        std::size_t set_start_idx = (addr % set_num) * LinesInSet;
        addr /= set_num;

        // Remain addr is tag, need to add 1 since empty tag is 0
        std::size_t tag = addr + 1;

        
        for (std::size_t set_el = 0;  set_el < LinesInSet; ++set_el) {
            cache_line &line = cache[set_start_idx + set_el];
            if (line.tag == tag) {
                // Found byte by offset
                uint8_t value = (line.value >> offset) % (sizeof(uint8_t) * 8);
                return {true, value};
            }
        }

        return {false, 0};
    }
};

int main() {
    set_assotiative_cache<> cache;

    std::vector<std::pair<uint64_t, uint64_t>> test_set;
    for (int i = 0; i < 1000; ++i) {
        test_set.emplace_back(i * 64, i);
    }

    for (const auto& test_case: test_set) {
        // std::cout << "place " << test_case.first << " value " << (int)test_case.second << std::endl;
        cache.push_line(test_case.first, test_case.second);
    }

    std::cout << std::endl;

    std::size_t found_count = 0;
    for (const auto& test_case: test_set) {
        auto result = cache.find(test_case.first);
        // std::cout << "find " << test_case.first << " value " << (int)result.second << " found " << std::boolalpha << result.first << std::endl;
        if (result.first) {
            ++found_count;
        }
    }

    std::cout << "Found count is " << found_count << std::endl; // Should be 256
}
