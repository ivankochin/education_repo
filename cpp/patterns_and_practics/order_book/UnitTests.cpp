#define BOOST_TEST_MODULE SimplifiedExchange test
#include <boost/test/included/unit_test.hpp>
// Please use a meaningful name here, ie.
#include "SimplifiedExchange.hpp"

#include <unordered_set>
#include <map>
#include <limits>

namespace Exchange { namespace Test {

// Some comments on what can be done to improve these test:
// - Check bestPrice changes on simplest cases with hardcoded checks to checks tests reference implementation
// - Create random generated tests with complex action chains and automatic checking
// - Check code coverage to understand what needs to be done more

struct Order {
    std::string symbol;
    Side side;
    Price price;
    Volume volume;
    UserReference reference;

    Order& SetSymbol(std::string newSymbol) {
        symbol = newSymbol;
        return *this;
    }
    Order& SetSide(Side newSide) {
        side = newSide;
        return *this;
    }
    Order& SetPrice(Price newPrice) {
        price = newPrice;
        return *this;
    }
    Order& SetVolume(Volume newVolume) {
        volume = newVolume;
        return *this;
    }
    Order& SetReference(UserReference newReference) {
        reference = newReference;
        return *this;
    }
};

/// This class sets up the necessary prerequisites for a unit test
class ExchangeFixtures
{
public:
    struct OrderInsertedEvent {
        UserReference userReference;
        InsertError insertError;
        OrderId orderId;
    };

    struct OrderDeletedEvent {
        OrderId orderId;
        DeleteError deleteError;
    };

    using OrdersSuite = std::vector<Order>;
    using ReferencesSet = std::unordered_set<UserReference>;

    ExchangeFixtures() {
        SetInsertedHandler();
        SetDeletedHandler();
        SetBestPriceHandler();
    }

    void OrderInsertedHandler(UserReference userReference, InsertError insertError, OrderId orderId) {
        insertedEvents.emplace_back(userReference, insertError, orderId);
    }

    void OrderDeletedHandler(OrderId orderId, DeleteError deleteError) {
        deletedEvents.emplace_back(orderId, deleteError);
    }

    void BestPriceCheckCountHandler(const std::string&, Price, Volume, Price, Volume) {
        ++bestPriceCallbackCount;
    }

    void InsertOrder(const Order& order) {
        exchange.InsertOrder(order.symbol, order.side, order.price, order.volume, order.reference);
    }

    void DeleteOrder(OrderId orderId) {
        exchange.DeleteOrder(orderId);
    }

    void ExpandOrdersForAllSides(std::vector<Order>& orders) {
        std::vector<Order> newOrders;
        auto getAnotherSide = [](Side side) {
            return (side == Side::Buy) ? Side::Sell : Side::Buy;
        };
        for (const auto& order: orders) {
            newOrders.emplace_back(order.symbol, getAnotherSide(order.side), order.price, order.volume, GetNewReference());
        }
        orders.insert(orders.end(), newOrders.begin(), newOrders.end());
    }

    void ExpandOrdersForAllSymbols(std::vector<Order>& orders) {
        std::vector<Order> newOrders;
        for (const auto& order: orders) {
            for (const auto& symbol: simplified::supportedStocks) {
                if (order.symbol != symbol) {
                    newOrders.emplace_back(symbol, order.side, order.price, order.volume, GetNewReference());
                }
            }
        }
        orders.insert(orders.end(), newOrders.begin(), newOrders.end());
    }

    void CheckAllInsertedEvents(InsertError errCode) {
        CheckInsertedEvents(errCode, std::begin(insertedEvents), std::end(insertedEvents));
    }

    template <typename EventsIt>
    void CheckInsertedEvents(InsertError errCode, EventsIt begin, EventsIt end) {
        ReferencesSet usedReferencesCopy = usedReferences;
        std::unordered_set<OrderId> ids;
        for (auto eventIt = begin; eventIt != end; ++eventIt) {
            // Check OrderIds are unique
            BOOST_REQUIRE(ids.insert(eventIt->orderId).second);
            // Check all references were delivered
            BOOST_CHECK_EQUAL(usedReferencesCopy.erase(eventIt->userReference), (std::size_t)1);
            // Check expected error code
            BOOST_CHECK_EQUAL(eventIt->insertError, errCode);
        }

        BOOST_CHECK_EQUAL(usedReferencesCopy.size(), usedReferences.size() - std::distance(begin, end));
    }

    void CheckDeletedEventsAllDeleted(DeleteError errCode) {
        std::unordered_set<OrderId> insertedIds;
        for (const auto& event: insertedEvents) {
            insertedIds.insert(event.orderId);
        }

        for (const auto& event: deletedEvents) {
            // Check all inserted events were deleted
            BOOST_CHECK_EQUAL(insertedIds.erase(event.orderId), (std::size_t)1);
            // Check expected error code
            BOOST_CHECK_EQUAL(event.deleteError, errCode);
        }
    }

    Order MakeDefaultOrder() {
        return {defaultSymbol, defaultSide, defaultPrice, defaultVolume, GetNewReference()};
    }

    UserReference GetNewReference() {
        auto result = referenceCounter++;
        usedReferences.emplace(result);
        return result;
    }

    void SetInsertedHandler() {
        using std::placeholders::_1;
        using std::placeholders::_2;
        using std::placeholders::_3;
        // Events are logged to a std::vector for later checking
        exchange.OnOrderInserted = std::bind(&ExchangeFixtures::OrderInsertedHandler, this, _1, _2, _3);
    }

    void SetDeletedHandler() {
        using std::placeholders::_1;
        using std::placeholders::_2;
        // Events are logged to a std::vector for later checking
        exchange.OnOrderDeleted = std::bind(&ExchangeFixtures::OrderDeletedHandler, this, _1, _2);
    }

    void SetBestPriceHandler() {
        using std::placeholders::_1;
        using std::placeholders::_2;
        using std::placeholders::_3;
        using std::placeholders::_4;
        using std::placeholders::_5;
        // Events are logged to a std::vector for later checking
        exchange.OnBestPriceChanged = std::bind(&ExchangeFixtures::BestPriceCheckCountHandler, this, _1, _2, _3, _4, _5);
    }

    std::vector<OrderInsertedEvent> insertedEvents;
    std::vector<OrderDeletedEvent> deletedEvents;

    // This is the code under test
    simplified::Exchange exchange;

    const Price defaultPrice = 100;
    const Volume defaultVolume = 10;
    const Side defaultSide = Side::Buy;
    const std::string defaultSymbol = simplified::supportedStocks.front();

    ReferencesSet usedReferences;
    UserReference referenceCounter = 1;

    std::size_t bestPriceCallbackCount = 0;
};

BOOST_FIXTURE_TEST_SUITE(ExchangeTests, ExchangeFixtures)

BOOST_AUTO_TEST_CASE(TestInvalidStockCode)
{
    std::vector<Order> ordersToTest = {MakeDefaultOrder().SetSymbol("XXX")};
    ExpandOrdersForAllSides(ordersToTest);

    std::for_each(std::begin(ordersToTest), std::end(ordersToTest),
        [&](const Order& order) { InsertOrder(order); });

    CheckAllInsertedEvents(InsertError::SymbolNotFound);
    BOOST_REQUIRE(deletedEvents.empty());
    BOOST_CHECK_EQUAL(bestPriceCallbackCount, (std::size_t)00);
}

BOOST_AUTO_TEST_CASE(TestInvalidPrice)
{
    std::vector<Order> ordersToTest = {MakeDefaultOrder().SetPrice(0)};
    ExpandOrdersForAllSides(ordersToTest);
    ExpandOrdersForAllSymbols(ordersToTest);

    std::for_each(std::begin(ordersToTest), std::end(ordersToTest),
        [&](const Order& order) { InsertOrder(order); });

    CheckAllInsertedEvents(InsertError::InvalidPrice);
    BOOST_REQUIRE(deletedEvents.empty());
    BOOST_CHECK_EQUAL(bestPriceCallbackCount, (std::size_t)00);
}

BOOST_AUTO_TEST_CASE(TestMaxPrice)
{
    Price maxPrice = std::numeric_limits<Price>::max();
    std::vector<Order> ordersToTest = {MakeDefaultOrder().SetPrice(maxPrice)};
    ExpandOrdersForAllSides(ordersToTest);
    ExpandOrdersForAllSymbols(ordersToTest);

    std::for_each(std::begin(ordersToTest), std::end(ordersToTest),
        [&](const Order& order) { InsertOrder(order); });

    CheckAllInsertedEvents(InsertError::OK);
    BOOST_REQUIRE(deletedEvents.empty());
    BOOST_CHECK_EQUAL(bestPriceCallbackCount, ordersToTest.size());
}

BOOST_AUTO_TEST_CASE(TestInvalidVolume)
{
    std::vector<Order> ordersToTest = {MakeDefaultOrder().SetVolume(0)};
    ExpandOrdersForAllSides(ordersToTest);
    ExpandOrdersForAllSymbols(ordersToTest);

    std::for_each(std::begin(ordersToTest), std::end(ordersToTest),
        [&](const Order& order) { InsertOrder(order); });

    CheckAllInsertedEvents(InsertError::InvalidVolume);
    BOOST_REQUIRE(deletedEvents.empty());
    BOOST_CHECK_EQUAL(bestPriceCallbackCount, (std::size_t)0);
}

BOOST_AUTO_TEST_CASE(TestMaxVolume)
{
    Volume maxVolume = std::numeric_limits<Volume>::max();
    std::vector<Order> ordersToTest = {MakeDefaultOrder().SetVolume(maxVolume)};
    ExpandOrdersForAllSides(ordersToTest);
    ExpandOrdersForAllSymbols(ordersToTest);

    std::for_each(std::begin(ordersToTest), std::end(ordersToTest),
        [&](const Order& order) { InsertOrder(order); });

    CheckAllInsertedEvents(InsertError::OK);
    BOOST_REQUIRE(deletedEvents.empty());
    BOOST_CHECK_EQUAL(bestPriceCallbackCount, ordersToTest.size());
}

BOOST_AUTO_TEST_CASE(TestVolumeOverflow)
{
    Volume maxVolume = std::numeric_limits<Volume>::max();
    std::vector<Order> ordersToTest = {MakeDefaultOrder().SetVolume(maxVolume)};
    ExpandOrdersForAllSides(ordersToTest);
    ExpandOrdersForAllSymbols(ordersToTest);

    std::for_each(std::begin(ordersToTest), std::end(ordersToTest),
        [&](const Order& order) { InsertOrder(order); });

    std::for_each(std::begin(ordersToTest), std::end(ordersToTest), [&](Order order) {
            // Get overflow by inserting the same order with different reference
            InsertOrder(order.SetReference(GetNewReference()));
        });

    auto firstEventIt = std::begin(insertedEvents);
    auto middleEventIt = std::next(firstEventIt, ordersToTest.size());
    CheckInsertedEvents(InsertError::OK, firstEventIt, middleEventIt);
    CheckInsertedEvents(InsertError::SystemError, middleEventIt, std::end(insertedEvents));
    BOOST_CHECK_EQUAL(bestPriceCallbackCount, ordersToTest.size());
}

BOOST_AUTO_TEST_CASE(TestValidInsert)
{
    std::vector<Order> ordersToTest = {MakeDefaultOrder()};
    ExpandOrdersForAllSides(ordersToTest);
    ExpandOrdersForAllSymbols(ordersToTest);

    std::for_each(std::begin(ordersToTest), std::end(ordersToTest),
        [&](const Order& order) { InsertOrder(order); });

    CheckAllInsertedEvents(InsertError::OK);
    BOOST_REQUIRE(deletedEvents.empty());
    BOOST_CHECK_EQUAL(bestPriceCallbackCount, ordersToTest.size());
}


BOOST_AUTO_TEST_CASE(TestRemoveFromEmptyExchange)
{
    OrderId orderId = 1;
    DeleteOrder(orderId);
    BOOST_REQUIRE(insertedEvents.empty());
    BOOST_CHECK_EQUAL(deletedEvents.size(), (std::size_t)1);

    OrderDeletedEvent event = deletedEvents.front();
    BOOST_CHECK_EQUAL(event.orderId, orderId);
    BOOST_CHECK_EQUAL(event.deleteError, DeleteError::OrderNotFound);
}

BOOST_AUTO_TEST_CASE(TestRemoveWrongOrderId)
{
    InsertOrder(MakeDefaultOrder());

    OrderId wrongOrderId = insertedEvents.front().orderId + 1;
    DeleteOrder(wrongOrderId);
    BOOST_CHECK_EQUAL(insertedEvents.size(), (std::size_t)1);
    BOOST_CHECK_EQUAL(deletedEvents.size(), (std::size_t)1);

    OrderDeletedEvent event = deletedEvents.front();
    BOOST_CHECK_EQUAL(event.orderId, wrongOrderId);
    BOOST_CHECK_EQUAL(event.deleteError, DeleteError::OrderNotFound);
}

BOOST_AUTO_TEST_CASE(TestDoubleRemoveWrongOrderId)
{
    InsertOrder(MakeDefaultOrder());

    OrderId wrongOrderId = insertedEvents.front().orderId + 1;
    DeleteOrder(wrongOrderId);
    DeleteOrder(wrongOrderId);
    BOOST_CHECK_EQUAL(insertedEvents.size(), (std::size_t)1);
    BOOST_CHECK_EQUAL(deletedEvents.size(), (std::size_t)2);

    std::for_each(std::begin(deletedEvents), std::end(deletedEvents), [&](const OrderDeletedEvent& event) {
        // Check all inserted events were deleted
        BOOST_CHECK_EQUAL(event.orderId, wrongOrderId);
        // Check expected error code
        BOOST_CHECK_EQUAL(event.deleteError, DeleteError::OrderNotFound);
    });
}

BOOST_AUTO_TEST_CASE(TestRemoveValidOrder)
{
    std::vector<Order> ordersToTest = {MakeDefaultOrder()};
    ExpandOrdersForAllSides(ordersToTest);
    ExpandOrdersForAllSymbols(ordersToTest);

    std::for_each(std::begin(ordersToTest), std::end(ordersToTest),
        [&](const Order& order) { InsertOrder(order); });

    std::for_each(std::begin(insertedEvents), std::end(insertedEvents),
        [&](const OrderInsertedEvent& event) { DeleteOrder(event.orderId); });

    BOOST_CHECK_EQUAL(insertedEvents.size(), ordersToTest.size());
    CheckDeletedEventsAllDeleted(DeleteError::OK);
}

BOOST_AUTO_TEST_CASE(TestDoubleRemoveValidOrder)
{
    InsertOrder(MakeDefaultOrder());
    DeleteOrder(insertedEvents.front().orderId);
    DeleteOrder(insertedEvents.front().orderId);
    BOOST_CHECK_EQUAL(insertedEvents.size(), (std::size_t)1);
    BOOST_CHECK_EQUAL(deletedEvents.size(), (std::size_t)2);

    const auto& validDeleteEvent = deletedEvents.front();
    BOOST_CHECK_EQUAL(validDeleteEvent.orderId, insertedEvents.front().orderId);
    BOOST_CHECK_EQUAL(validDeleteEvent.deleteError, DeleteError::OK);

    const auto& wrongDeleteEvent = deletedEvents.back();
    BOOST_CHECK_EQUAL(wrongDeleteEvent.orderId, insertedEvents.front().orderId);
    BOOST_CHECK_EQUAL(wrongDeleteEvent.deleteError, DeleteError::OrderNotFound);
}

BOOST_AUTO_TEST_SUITE_END()

class ExchangeFixturesBestPrice: public ExchangeFixtures
{
public:
    struct BestPriceEvent {
        std::string symbol;
        Price bestBid;
        Volume totalBidVolume;
        Price bestAsk;
        Volume totalAskVolume;
    };

    template <typename Comp>
    using PriceMap = std::multimap<Price, Volume, Comp>;

    ExchangeFixturesBestPrice() {
        SetBestPriceHandler();
        SetInsertedTrackHandler();
    }

    void InsertOrder(const Order& order) {
        // Insert volume from priceMap
        auto insertPrice = [](auto& priceMap, Price price, Volume volume) {
            priceMap.emplace(price, volume);
        };
        ordersToInsert[order.reference] = order;
        if (order.side == Side::Buy) {
            insertPrice(orderBook[order.symbol].first, order.price, order.volume);
        } else {
            insertPrice(orderBook[order.symbol].second, order.price, order.volume);
        }

        ExchangeFixtures::InsertOrder(order);
    }

    void DeleteOrder(OrderId orderId) {
        Order& order = insertedOrders[orderId];

        // Remove volume from priceMap
        auto removeVolume = [](auto& priceMap, Price price, Volume volume) {
            auto priceVolumes = priceMap.equal_range(price);
            for (auto volumeIt = priceVolumes.first; volumeIt != priceVolumes.second; ++volumeIt) {
                if (volumeIt->second == volume) {
                    priceMap.erase(volumeIt);
                    return;
                }
            }
        };
        ordersToInsert[order.reference] = order;
        if (order.side == Side::Buy) {
            removeVolume(orderBook[order.symbol].first, order.price, order.volume);
        } else {
            removeVolume(orderBook[order.symbol].second, order.price, order.volume);
        }

        ExchangeFixtures::DeleteOrder(orderId);
    }

    template<typename PriceMapT>
    std::pair<Price, Volume> GetBestPriceFromMap(const PriceMapT& priceMap) {
        if (priceMap.empty()) return {0, 0};

        Price bestPrice = priceMap.begin()->first;
        Volume totalVolume = 0;
        auto bestPriceVolumes = priceMap.equal_range(bestPrice);
        std::for_each(bestPriceVolumes.first, bestPriceVolumes.second, [&](auto priceToVolume) {
            totalVolume += priceToVolume.second;
        });

        return {bestPrice, totalVolume};
    }

    BestPriceEvent GetCurrentBestPrice(const std::string& symbol) {
        auto bidBestPrice = GetBestPriceFromMap(orderBook[symbol].first);
        auto askBestPrice = GetBestPriceFromMap(orderBook[symbol].second);

        return {symbol, bidBestPrice.first, bidBestPrice.second, askBestPrice.first, askBestPrice.second};
    }

    void BestPriceHandler(const std::string& symbol, Price bestBid, Volume totalBidVolume, Price bestAsk, Volume totalAskVolume) {
        bestPriceEvents.emplace_back(symbol, bestBid, totalBidVolume, bestAsk, totalAskVolume);
        BestPriceEvent bestPriceReference = GetCurrentBestPrice(symbol);
        BestPriceEvent& bestPrice= bestPriceEvents.back();

        BOOST_CHECK_EQUAL(bestPriceReference.symbol, bestPrice.symbol);
        BOOST_CHECK_EQUAL(bestPriceReference.bestBid, bestPrice.bestBid);
        BOOST_CHECK_EQUAL(bestPriceReference.totalBidVolume, bestPrice.totalBidVolume);
        BOOST_CHECK_EQUAL(bestPriceReference.bestAsk, bestPrice.bestAsk);
        BOOST_CHECK_EQUAL(bestPriceReference.totalAskVolume, bestPrice.totalAskVolume);
    }

    void OrderInsertedHandlerTrackOrders(UserReference userReference, InsertError insertError, OrderId orderId) {
        ExchangeFixtures::OrderInsertedHandler(userReference, insertError, orderId);
        insertedOrders[orderId] = ordersToInsert[userReference];
    }

    void SetBestPriceHandler() {
        using std::placeholders::_1;
        using std::placeholders::_2;
        using std::placeholders::_3;
        using std::placeholders::_4;
        using std::placeholders::_5;
        // Events are logged to a std::vector for later checking
        exchange.OnBestPriceChanged = std::bind(&ExchangeFixturesBestPrice::BestPriceHandler, this, _1, _2, _3, _4, _5);
    }

    void SetInsertedTrackHandler() {
        using std::placeholders::_1;
        using std::placeholders::_2;
        using std::placeholders::_3;
        // Events are logged to a std::vector for later checking
        exchange.OnOrderInserted = std::bind(&ExchangeFixturesBestPrice::OrderInsertedHandlerTrackOrders, this, _1, _2, _3);
    }

    std::vector<BestPriceEvent> bestPriceEvents;
    std::unordered_map<UserReference, Order> ordersToInsert;
    std::unordered_map<OrderId, Order> insertedOrders;
    // Reference implementation for orderBook, first PriceMap is for Buy, second is for Sell
    std::unordered_map<std::string, std::pair<PriceMap<std::greater<Price>>, PriceMap<std::less<Price>>>> orderBook;
};

BOOST_FIXTURE_TEST_SUITE(ExchangeTestsBestPrice, ExchangeFixturesBestPrice)

BOOST_AUTO_TEST_CASE(TestInsertBestPriceChange)
{
    std::vector<Order> ordersToTest = {MakeDefaultOrder()};
    ExpandOrdersForAllSides(ordersToTest);
    ExpandOrdersForAllSymbols(ordersToTest);

    std::for_each(std::begin(ordersToTest), std::end(ordersToTest),
        [&](const Order& order) { InsertOrder(order); });

    // In that test all the orders placements reports best prices
    BOOST_CHECK_EQUAL(ordersToTest.size(), bestPriceEvents.size());
}

BOOST_AUTO_TEST_CASE(TestRemoveBestPriceChange)
{
    std::vector<Order> ordersToTest = {MakeDefaultOrder()};
    ExpandOrdersForAllSides(ordersToTest);
    ExpandOrdersForAllSymbols(ordersToTest);

    std::for_each(std::begin(ordersToTest), std::end(ordersToTest), [&](const Order& order) {
        InsertOrder(order);
        DeleteOrder(insertedEvents.back().orderId);
    });

    // In that test all the orders placements and deletions reports best prices
    BOOST_CHECK_EQUAL(ordersToTest.size() * 2, bestPriceEvents.size());
}

BOOST_AUTO_TEST_CASE(TestVolumeBestPriceChange)
{
    std::vector<Order> ordersToTest = {MakeDefaultOrder()};
    ExpandOrdersForAllSides(ordersToTest);
    ExpandOrdersForAllSymbols(ordersToTest);

    std::for_each(std::begin(ordersToTest), std::end(ordersToTest), [&](Order& order) {
        InsertOrder(order);
        InsertOrder(order.SetReference(GetNewReference()));
    });

    // In that test all the orders placements and deletions reports best prices
    BOOST_CHECK_EQUAL(ordersToTest.size() * 2, bestPriceEvents.size());
}

BOOST_AUTO_TEST_CASE(TestVolumeRemoveBestPriceChange)
{
    std::vector<Order> ordersToTest = {MakeDefaultOrder()};
    ExpandOrdersForAllSides(ordersToTest);
    ExpandOrdersForAllSymbols(ordersToTest);

    std::for_each(std::begin(ordersToTest), std::end(ordersToTest), [&](Order& order) {
        InsertOrder(order);
        InsertOrder(order.SetReference(GetNewReference()));
    });

    std::for_each(std::begin(insertedEvents), std::end(insertedEvents), [&](const auto& event) {
        DeleteOrder(event.orderId);
    });

    // In that test all the orders placements and deletions reports best prices
    BOOST_CHECK_EQUAL(ordersToTest.size() * 4, bestPriceEvents.size());
}

BOOST_AUTO_TEST_CASE(TestChangeExistingPriceChange)
{
    std::vector<Order> ordersToTest = {MakeDefaultOrder()};
    ExpandOrdersForAllSides(ordersToTest);
    ExpandOrdersForAllSymbols(ordersToTest);

    std::for_each(std::begin(ordersToTest), std::end(ordersToTest), [&](Order& order) {
        InsertOrder(order);
        Price better_price = (order.side == Side::Buy) ? (order.price * 2) : (order.price / 2);
        InsertOrder(order.SetPrice(better_price).SetReference(GetNewReference()));
    });

    // Second insertion doesn't update the best price
    BOOST_CHECK_EQUAL(ordersToTest.size() * 2, bestPriceEvents.size());
}

BOOST_AUTO_TEST_CASE(TestVolumesNoPriceChange)
{
    std::vector<Order> ordersToTest = {MakeDefaultOrder()};
    ExpandOrdersForAllSides(ordersToTest);
    ExpandOrdersForAllSymbols(ordersToTest);

    std::for_each(std::begin(ordersToTest), std::end(ordersToTest), [&](Order& order) {
        InsertOrder(order);
        Price worse_price = (order.side == Side::Buy) ? (order.price / 2) : (order.price * 2);
        InsertOrder(order.SetPrice(worse_price).SetReference(GetNewReference()));
    });

    // Second insertion doesn't update the best price
    BOOST_CHECK_EQUAL(ordersToTest.size(), bestPriceEvents.size());
}

BOOST_AUTO_TEST_SUITE_END()

class ExchangeFixturesCallbacks: public ExchangeFixtures
{
    public:
    ExchangeFixturesCallbacks() {}
};

BOOST_FIXTURE_TEST_SUITE(ExchangeTestsCallbacks, ExchangeFixturesCallbacks)

BOOST_AUTO_TEST_CASE(TestInsertWOCallback)
{
    SetBestPriceHandler();

    Order order = MakeDefaultOrder();
    InsertOrder(order);
    BOOST_CHECK_EQUAL(bestPriceCallbackCount, (std::size_t)1);
}

BOOST_AUTO_TEST_CASE(TestDeleteWOCallback)
{
    SetInsertedHandler();
    SetBestPriceHandler();

    Order order = MakeDefaultOrder();
    InsertOrder(order);
    BOOST_CHECK_EQUAL(bestPriceCallbackCount, (std::size_t)1);
    DeleteOrder(insertedEvents.front().orderId);
    BOOST_CHECK_EQUAL(bestPriceCallbackCount, (std::size_t)2);

    UserReference newReference = GetNewReference();
    InsertOrder(order.SetReference(newReference));
    // New inserted and bestPrice events were created
    BOOST_CHECK_EQUAL(bestPriceCallbackCount, (std::size_t)3);
    BOOST_CHECK_EQUAL(insertedEvents.size(), (std::size_t)2);

    BOOST_CHECK_NE(insertedEvents.back().orderId, insertedEvents.front().orderId);
    BOOST_CHECK_EQUAL(insertedEvents.back().userReference, newReference);
    BOOST_CHECK_EQUAL(insertedEvents.back().insertError, InsertError::OK);
}

BOOST_AUTO_TEST_SUITE_END()


} } // { namespace Exchange { namespace Test {
