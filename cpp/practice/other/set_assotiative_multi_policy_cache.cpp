#include <vector>
#include <unordered_map>
#include <array>
#include <list>
#include <iostream>
#include <cassert>
#include <cstdlib>
#include <cstdint>

enum class ReplacementPolicy {LRU, MRU};

template <ReplacementPolicy Policy>
struct erase_order {
    template<typename List>
    typename List::value_type operator()(List& order) {
        // LRU erase
        auto result = order.back();
        order.pop_back();
        return result;
    }
};

template <>
struct erase_order <ReplacementPolicy::MRU> {
    template<typename List>
    typename List::value_type operator()(List& order) {
        // MRU erase
        auto result = order.front();
        order.pop_front();
        return result;
    }
};

template <typename Key, typename Value, ReplacementPolicy Policy = ReplacementPolicy::LRU,
          std::size_t CacheSize = 16, std::size_t SetSize = 4>
class set_assotiative_cache {
    constexpr static std::size_t set_num = CacheSize / SetSize; // 8 for default configuration
    struct value_type;
    using hash_table = std::unordered_map<Key, value_type>;
    using order_list = std::list<typename hash_table::iterator>;
    struct value_type {
        Value value;
        typename order_list::iterator order_it;
    };

    struct cache_set {
        order_list order{};
        hash_table cache{SetSize}; // Setting bucket count to SetSize guarantees iterator validity
        
        void push(const Key& key, const Value& value) {
            auto entry = cache.find(key);

            // Already in cache
            if (entry != std::end(cache)) {
                entry->second.value = value;
                order.erase(entry->second.order_it);
                order.emplace_front(entry);
                return;
            }

            if (cache.size() >= SetSize) {
                auto cache_entry = erase_order<Policy>{}(order);
                std::cout << "-- replace " << cache_entry->first << std::endl;
                cache.erase(cache_entry);
            }

            // Place wrong order_it to update it later
            entry = cache.emplace(key, value_type{value, std::begin(order)}).first;
            order.emplace_front(entry);
            entry->second.order_it = std::begin(order);
        }

        std::pair<bool, Value> get(Key key) {
            auto entry = cache.find(key);
            if (entry != std::end(cache)) {
                order.erase(entry->second.order_it);
                order.emplace_front(entry);

                return {true, entry->second.value};
            }

            return {false, Value{}};
        }
    };

    std::array<cache_set, set_num> sets_array;
public:
    void push(const Key& key, const Value& value) {
        sets_array[key % set_num].push(key, value);
    }

    std::pair<bool, Value> get(Key key) {
        return sets_array[key % set_num].get(key);
    }
};

int main() {
    set_assotiative_cache<uint64_t, uint64_t, ReplacementPolicy::MRU> cache;

    std::vector<std::pair<uint64_t, uint64_t>> test_set;
    for (int i = 100; i > 0; --i) {
        test_set.emplace_back(i * 2, i);
    }

    for (const auto& test_case: test_set) {
        std::cout << "place " << test_case.first << " value " << (int)test_case.second << std::endl;
        cache.push(test_case.first, test_case.second);
    }

    std::cout << std::endl;

    std::size_t found_count = 0;
    for (const auto& test_case: test_set) {
        auto result = cache.get(test_case.first);
        // std::cout << "find " << test_case.first << " value " << (int)result.second << " found " << std::boolalpha << result.first << std::endl;
        if (result.first) {
            ++found_count;
        }
    }

    std::cout << "Found count is " << found_count << std::endl; // Should be 256
}
