#include "../core/Event.h"
#include "../core/Types.h"
#include <memory>
#include <vector>

namespace backtester {

class IStrategy {
 public:
    virtual ~IStrategy() = default;

    virtual void Initialize(const Strategy& config);

    virtual std::unique_ptr<StrategySignalEvent> OnMarketEvent(
        const MarketByOrderEvent& event,
        const std::vector<BidAskPair> ob_snapshot) ;  // return signal if generated, else nullptr

    virtual void OnFill(const StrategyFillEvent& fill) ;  // update internal state (e.g., position)

    virtual void OnEndOfDay(uint64_t timestamp) ;
    std::string GetId() const { return strategy_id_; }
 protected:
    std::string strategy_id_;

};

}