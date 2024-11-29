#pragma once

#include <vector>
#include <unordered_map>

#include "IExchange.hpp"

namespace simplified {
namespace details {

// Questions:
// - Should issues like absense of memory to allocate be handled as SystemError and save 
//   valid system state (strong exception guarantees)?
// - Should we consider absence of BestPriceHanlder as reason to rid of bestPrice calculation?
//   From my point of view some optimization can be done (if we take into account that we don't
//   need market matchin functionality)
// Points:
// - Next implementation stage can be detailed logging.
// - Flat map/set structures implementation can improve cache locality

// No need to use perfect forwarding for Args since all Args are supposed to be integral types
template <typename Func, typename... Args>
static void ExecuteCallback(Func callback, Args... args) {
    // Prevent throwing std::bad_function_call in case of unset callback
    if (callback) {
        callback(args...);
    }
}

struct MetaInfo {
    std::string symbol;
    Price price;
};

struct VolumeStorage {
    InsertError AddVolume(OrderId orderId, Volume volume) {
        Volume NewVolume = TotalVolume + volume;
        if (NewVolume < TotalVolume) {
            // Total volume overflow
            return InsertError::SystemError;
        }

        Volumes[orderId] = volume;
        TotalVolume = NewVolume;
        return InsertError::OK;
    }

    DeleteError RemoveVolume(OrderId orderId) {
        auto volumeIt = Volumes.find(orderId);
        if (volumeIt == std::end(Volumes)) return DeleteError::SystemError;

        TotalVolume -= volumeIt->second;
        Volumes.erase(volumeIt);
        return DeleteError::OK;
    }

    Volume GetTotalVolume() const {
        return TotalVolume;
    }

    bool empty() const {
        return Volumes.empty();
    }
private:
    std::unordered_map<OrderId, Volume> Volumes;
    Volume TotalVolume = 0;
};

class OrderBook {
    using OrderStorage = std::unordered_map<Price, VolumeStorage>;
    // Seems like best prices storage can be stored in one type with std::less but Bids
    // best would be acquired as std::prev(std::end(pricesStorage))
    using BidsPricesStorage = std::set<Price, std::greater<Price>>;
    using AsksPricesStorage = std::set<Price, std::less<Price>>;

    template <typename Prices>
    std::pair<Price, Volume> GetSideBestPriceInfo(const Prices& prices, const OrderStorage& orders) const {
        Price bestPrice = 0;
        Volume bestVolume = 0;
        if (prices.size() > 0) {
            bestPrice = *std::begin(prices);
            // orders should contain bestPrice
            bestVolume = orders.find(bestPrice)->second.GetTotalVolume();
        }

        return {bestPrice, bestVolume};
    }

    OrderStorage& Orders(Side side) {
        return (side == Side::Buy) ? BidsOrders : AsksOrders;
    }

    bool InsertPrice(Side side, Price price) {
        auto insertPrice = [](auto& prices, Price price) -> bool {
            auto currentBestPriceIt = std::begin(prices);
            auto [priceIt, result]= prices.insert(price);

            // Best price volume was updated
            if (!result && price == *currentBestPriceIt) {
                return true;
            }

            // Best price was updated
            if (std::begin(prices) != currentBestPriceIt) {
                return true;
            }
            return false;
        };

        if (side == Side::Buy) {
            return insertPrice(BidsPrices, price);
        }
        return insertPrice(AsksPrices, price);
    }

    void RemovePrice(Side side, Price price) {
        auto removePrice = [](auto& prices, Price price) {
            prices.erase(price);
        };

        if (side == Side::Buy) {
            removePrice(BidsPrices, price);
        } else {
            removePrice(AsksPrices, price);
        }
    }

    bool IsBestPrice(Side side, Price price) {
        auto isBestPrice = [](const auto& prices, Price price) {
            return *std::begin(prices) == price;
        };

        if (side == Side::Buy) {
            return isBestPrice(BidsPrices, price);
        }
        return isBestPrice(AsksPrices, price);
    }

public:
    std::tuple<Price, Volume, Price, Volume> GetBestPriceInfo() const {
        auto [bestBidPrice, bestBidVolume] = GetSideBestPriceInfo(BidsPrices, BidsOrders);
        auto [bestAskPrice, bestAskVolume] = GetSideBestPriceInfo(AsksPrices, AsksOrders);

        return {bestBidPrice, bestBidVolume, bestAskPrice, bestAskVolume};
    }

    // Returns pair of error code and indication whether best price was updated
    std::pair<InsertError, bool> PlaceOrder(Side side, Price price, Volume volume, OrderId orderId) {
        if (price == 0) return {InsertError::InvalidPrice, false};
        if (volume == 0) return {InsertError::InvalidVolume, false};

        auto errCode = Orders(side)[price].AddVolume(orderId, volume);
        if (errCode != InsertError::OK) {
            return {errCode, false};
        }

        return {InsertError::OK, InsertPrice(side, price)};
    }

    std::pair<DeleteError, bool> RemoveOrder(OrderId orderId, Side side, Price price) {
        bool bestPrice = IsBestPrice(side, price);
        DeleteError errCode = Orders(side)[price].RemoveVolume(orderId);
        if (errCode != DeleteError::OK) {
            return {errCode, false};
        }

        if (Orders(side)[price].empty()) {
            Orders(side).erase(price);
            RemovePrice(side, price);
        }

        return {DeleteError::OK, bestPrice};
    }
private:
    OrderStorage BidsOrders, AsksOrders;
    BidsPricesStorage BidsPrices;
    AsksPricesStorage AsksPrices;
};
} // namespace details

const inline std::vector<std::string> supportedStocks = {"AAPL", "MSFT", "GOOG"};

class Exchange : public IExchange {
public:
    virtual ~Exchange() {}

    Exchange() {
        std::for_each(std::begin(supportedStocks), std::end(supportedStocks),
            [&](std::string stockName){
                OrderBooks.emplace(stockName, details::OrderBook{});
            });
    }

    virtual void InsertOrder(const std::string& symbol, Side side, Price price, Volume volume,
                             UserReference userReference) override {
        uint64_t orderId = GetOrderId(side);

        auto orderBookIt = OrderBooks.find(symbol);
        if (orderBookIt == std::end(OrderBooks)) {
            details::ExecuteCallback(OnOrderInserted, userReference, InsertError::SymbolNotFound, orderId);
            return;
        }

        auto [errCode, reportBestPrice] = orderBookIt->second.PlaceOrder(side, price, volume, orderId);
        details::ExecuteCallback(OnOrderInserted, userReference, errCode, orderId);

        if (errCode == InsertError::OK) {
            OrderMetaInfo[orderId] = {symbol, price};
            if (reportBestPrice) {
                auto [bestBid, totalBidVolume, bestAsk, totalAskVolume] = orderBookIt->second.GetBestPriceInfo();
                details::ExecuteCallback(OnBestPriceChanged, symbol, bestBid, totalBidVolume, bestAsk, totalAskVolume);
            }
        }
    }

    virtual void DeleteOrder(OrderId orderId) override {
        auto metaInfoIt = OrderMetaInfo.find(orderId);
        if (metaInfoIt == std::end(OrderMetaInfo)) {
            details::ExecuteCallback(OnOrderDeleted, orderId, DeleteError::OrderNotFound);
            return;
        }
        details::MetaInfo& metaInfo = metaInfoIt->second;

        auto [errCode, reportBestPrice] = OrderBooks[metaInfo.symbol].RemoveOrder(orderId, GetSide(orderId), metaInfo.price);
        details::ExecuteCallback(OnOrderDeleted, orderId, errCode);

        if (errCode == DeleteError::OK) {
            OrderMetaInfo.erase(orderId);
            if (reportBestPrice) {
                auto [bestBid, totalBidVolume, bestAsk, totalAskVolume] = OrderBooks[metaInfo.symbol].GetBestPriceInfo();
                details::ExecuteCallback(OnBestPriceChanged, metaInfo.symbol, bestBid, totalBidVolume, bestAsk, totalAskVolume);
            }
        }
    }

private:
    OrderId GetOrderId(Side side) {
        OrderId *orderId = &AsksOrderIdCounter;
        if (side == Side::Buy) {
            orderId = &BidsOrderIdCounter;
        }
        *orderId += 2;
        return *orderId;
    }

    Side GetSide(OrderId orderId) {
        return (orderId % 2) ? Side::Sell : Side::Buy;
    }

    std::unordered_map<std::string, details::OrderBook> OrderBooks;

    // Order side can also be stored as separate field in this map,
    // but I found solution with dumping side to orderId logic more interesting
    // since it reduces the memory footprint. But now solution supports
    // only up to 2^63 bids and asks separately
    std::unordered_map<OrderId, details::MetaInfo> OrderMetaInfo;
    OrderId BidsOrderIdCounter = 0;
    OrderId AsksOrderIdCounter = 1;
};

} // namespace simplified