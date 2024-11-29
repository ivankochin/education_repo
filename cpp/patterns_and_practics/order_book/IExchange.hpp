#pragma once

#include <functional>
#include <string>
#include <ostream>

enum class Side { Buy, Sell };
using Price = unsigned;
using Volume = unsigned;
using UserReference = int;
using OrderId = int;
enum class InsertError { OK, SymbolNotFound, InvalidPrice, InvalidVolume, SystemError };
enum class DeleteError { OK, OrderNotFound, SystemError };

class IExchange
{
public:
    virtual ~IExchange() {}

    virtual void InsertOrder(
        const std::string& symbol,
        Side side,
        Price price,
        Volume volume,
        UserReference userReference
        ) = 0;
    virtual void DeleteOrder(
        OrderId orderId
        ) = 0;

    using OrderInsertedFunction = std::function<void (UserReference, InsertError, OrderId)>;
    OrderInsertedFunction OnOrderInserted;

    using OrderDeletedFunction = std::function<void (OrderId, DeleteError)>;
    OrderDeletedFunction OnOrderDeleted;
    using BestPriceChangedFunction = std::function<void (
        const std::string& symbol,
        Price bestBid,
        Volume totalBidVolume,
        Price bestAsk,
        Volume totalAskVolume)>;
    BestPriceChangedFunction OnBestPriceChanged;
};

inline std::ostream& operator<<(std::ostream& os, InsertError er)
{
    switch (er)
    {
    case InsertError::OK:
        return os << "OK";
    case InsertError::SymbolNotFound:
        return os << "SymbolNotFound";
    case InsertError::InvalidVolume:
        return os << "InvalidVolume";
    case InsertError::SystemError:
        return os << "SystemError";
    default:
        throw std::runtime_error("Unhandled enum");
    }
}
inline std::ostream& operator<<(std::ostream& os, DeleteError er)
{
    switch (er)
    {
    case DeleteError::OK:
        return os << "OK";
    case DeleteError::OrderNotFound:
        return os << "OK";
    case DeleteError::SystemError:
        return os << "SystemError";
    default:
        throw std::runtime_error("Unhandled enum");
    }
}
