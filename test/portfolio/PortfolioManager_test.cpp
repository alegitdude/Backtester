#include "../../include/portfolio/PortfolioManager.h"
#include "../../include/core/Types.h"
#include "../../include/core/Event.h"
#include <gtest/gtest.h>
#include <memory>
#include <vector>

namespace backtester {

const Position kEmptyPosition = {0, 0, 0, 0};

class PortfolioManagerTest : public ::testing::Test {
 protected:
    const uint32_t kFutInstrumentId = 1;
    const std::string kSymbolStr = "ESZ5";
    const int64_t kInitialCash = 100000'000'000'000;
    BidAskPair kEmptyBbo = {0, 0, 0, 0, 0, 0};
    // Config objects
    AppConfig config_fut_;
    AppConfig config_stk_;

    BidAskPair kValidEsBidAskPair = {4000'000'000'000, 0 ,0, 4000'250'000'000, 0, 0};

    void SetUp() override {
        // Futures Config 
        config_fut_.initial_cash = kInitialCash;
        config_fut_.traded_instruments = {{
            .instrument_id = kFutInstrumentId,
            .instrument_type = InstrumentType::FUT,
            .tick_size = 250'000'000,
            .tick_value = 12'500'000'000, // $50 per point 
            .margin_req = 5000'000'000'000 
        }};
        config_fut_.risk_limits.max_position_size = 10'000'000'000;
        config_fut_.risk_limits.max_drawdown_pct = 0'100'000'000; 
        config_fut_.risk_limits.max_risk_per_trade_pct = 0'020'000'000;

        // Stock Config 
        config_stk_.initial_cash = kInitialCash;
        config_stk_.traded_instruments = {{
            .instrument_id = 2,
            .instrument_type = InstrumentType::STOCK,
            .tick_size = 0'010'000'000,
            .tick_value = 0'010'000'000, // 1:1 value
            .margin_req = 0   // Cash account 
        }};
    }

    StrategySignalEvent CreateSignal(uint64_t timestamp, 
        int32_t signal_id, uint32_t instrument_id, SignalType type, int64_t price, uint32_t qty) {
        return StrategySignalEvent(
            timestamp,
            EventType::kStrategySignal,
            signal_id,
            "TestStrat",
            //1000,           // timestamp
            kFutInstrumentId,  // symbol/id
            type,
            price,
            qty
        );
    }
};

// =============================================================================
// MARK: Initialization Tests
// =============================================================================

TEST_F(PortfolioManagerTest, InitializationTest_Fut) {
    PortfolioManager pm(config_fut_);
    const std::unordered_map<uint32_t, BidAskPair>& cur_Bbos = {};

    EXPECT_EQ(pm.GetCash(), kInitialCash);
    EXPECT_EQ(pm.GetRealizedPnL(), 0);
    EXPECT_EQ(pm.GetUnrealizedPnL(kEmptyPosition, kEmptyBbo), 0);
    EXPECT_FALSE(pm.HasPosition(kFutInstrumentId));
    EXPECT_EQ(pm.GetPositionQty(kFutInstrumentId), 0);
    // Initial buying power should be Initial Cash (no margin used)
    EXPECT_EQ(pm.GetBuyingPowerAvailable(cur_Bbos), kInitialCash);
    EXPECT_EQ(pm.GetPositionByInstrId(1), kEmptyPosition);
    EXPECT_EQ(pm.GetDelta(kFutInstrumentId, kEmptyBbo), 0);
    EXPECT_EQ(pm.GetTotalPortfolioDelta(cur_Bbos), 0);
}   

// =============================================================================
// MARK: Risk Gate Tests (RequestOrder)
// =============================================================================

TEST_F(PortfolioManagerTest, RiskGate_ValidEntry) {
    PortfolioManager pm(config_fut_);
    
    int64_t price = 4000'000'000'000; 
   

    auto signal = CreateSignal(1000, 5, 1, SignalType::kBuySignal, 4000'250'000'000, 1);

    std::unordered_map<uint32_t, BidAskPair> market_prices = {{kFutInstrumentId, kValidEsBidAskPair}};

    auto order = pm.RequestOrder(&signal, market_prices);

    ASSERT_NE(order, nullptr);
    EXPECT_EQ(order->type, EventType::kStrategyOrderAdd);
    EXPECT_EQ(order->side, OrderSide::kBid);
    EXPECT_DOUBLE_EQ(order->quantity, 1);
}

TEST_F(PortfolioManagerTest, RiskGate_RejectsInvalidTick) {
    AppConfig weird_tick = config_fut_;
    weird_tick.traded_instruments[0].tick_size = 70'000'000; //.25 % .07 != 0
    PortfolioManager pm_weird(weird_tick);
    
    auto signal = CreateSignal(1000, 5, 1, SignalType::kBuySignal, 4000'250'000'000, 1);
    auto order = pm_weird.RequestOrder(&signal, {{kFutInstrumentId, kValidEsBidAskPair}});
    
    EXPECT_EQ(order, nullptr) << "Should reject price 4000.25 when tick size is .7";
}

TEST_F(PortfolioManagerTest, RiskGate_InsufficientBuyingPower) {
    PortfolioManager pm(config_fut_);
    
    // Cash 100k. Margin 5k. Max contracts ~20.
    // Try to buy 50.
    auto signal = CreateSignal(1000, 5, 1, SignalType::kBuySignal, 4000'250'000'000, 50);
    auto order = pm.RequestOrder(&signal, {{kFutInstrumentId, kValidEsBidAskPair}});

    EXPECT_EQ(order, nullptr) << "Should reject order requiring 250k margin with only 100k cash";
}

TEST_F(PortfolioManagerTest, RiskGate_PositionLimits) {
    PortfolioManager pm(config_fut_);
    // Limit is 10.
    
    // 1. Fill a position of 10.
    auto fill = std::make_unique<StrategyFillEvent>(
        100,
        1,
        kFutInstrumentId,
        OrderSide::kBid,
        4000'000'000'000,
        10,
        "TestStrat"                 
    );
    //StrategyFillEvent fill = {100, 1, kFutInstrumentId, OrderSide::kBid, 4000'000'000'000, 10, 1};
    pm.ProcessFill(*fill);
    
    // 2. Try to buy 1 more.
    auto signal = CreateSignal(1000, 5, 1, SignalType::kBuySignal, 4000'000'000'000, 50);

    //auto signal = CreateSignal(SignalType::kBuySignal, 4000, 1);
    auto order = pm.RequestOrder(&signal, {{kFutInstrumentId, kValidEsBidAskPair}});

    EXPECT_EQ(order, nullptr) << "Should reject order exceeding max position limit";
}

// // ==========================================================================
// // MARK: Execution & PnL Tests (Futures)
// // ==========================================================================

TEST_F(PortfolioManagerTest, Execution_OpenAndClose_Profit) {
    PortfolioManager pm(config_fut_);

    // 1. Open Long 1 @ 4000
    auto fill = std::make_unique<StrategyFillEvent>(
        100,
        1,
        kFutInstrumentId,
        OrderSide::kBid,
        4000'000'000'000,
        1,
        "TestStrat"                 
    );
    pm.ProcessFill(*fill);

    EXPECT_EQ(pm.GetPositionQty(kFutInstrumentId), 1);
    EXPECT_DOUBLE_EQ(pm.GetCash(), kInitialCash); // Cash doesn't change on open in this model

    // 2. Close Long 1 @ 4010
    // Profit calc: (4010 - 4000) = 10 points.
    // Tick size 0.25 -> 40 ticks.
    // Tick val 12.50 -> 40 * 12.50 = $500 Profit.
    auto fill_close = std::make_unique<StrategyFillEvent>(
        100,
        1,
        kFutInstrumentId,
        OrderSide::kAsk,
        4010'000'000'000,
        1,
        "TestStrat"                 
    );
    pm.ProcessFill(*fill_close);

    EXPECT_EQ(pm.GetPositionQty(kFutInstrumentId), 0);
    EXPECT_EQ(pm.GetRealizedPnL(), 500'000'000'000);
    EXPECT_EQ(pm.GetCash(), kInitialCash + 500'000'000'000);
}

TEST_F(PortfolioManagerTest, Execution_OpenAndClose_Loss) {
    PortfolioManager pm(config_fut_);

    // 1. Open Short 1 @ 4000
    auto fill_open = std::make_unique<StrategyFillEvent>(
        100,
        1,
        kFutInstrumentId,
        OrderSide::kAsk,
        4000'000'000'000,
        1,
        "TestStrat"                 
    );
    pm.ProcessFill(*fill_open);

    EXPECT_EQ(pm.GetPositionQty(kFutInstrumentId), -1);

    // 2. Close Short 1 @ 4005 
    // Loss: 5 points * (1/0.25) * 12.5 = 20 * 12.5 = $250 Loss.
    auto fill_close = std::make_unique<StrategyFillEvent>(
        100,
        1,
        kFutInstrumentId,
        OrderSide::kBid,
        4005'000'000'000,
        1,
        "TestStrat"                 
    );
    pm.ProcessFill(*fill_close);

    EXPECT_EQ(pm.GetPositionQty(kFutInstrumentId), 0);
    EXPECT_EQ(pm.GetRealizedPnL(), -250'000'000'000);
    EXPECT_EQ(pm.GetCash(), kInitialCash - 250'000'000'000);
}

TEST_F(PortfolioManagerTest, Execution_PositionFlip) {
    PortfolioManager pm(config_fut_);

    // 1. Open Long 1 @ 4000
    auto fill_long = std::make_unique<StrategyFillEvent>(
        100,
        1,
        kFutInstrumentId,
        OrderSide::kBid,
        4000'000'000'000,
        1,
        "TestStrat"                 
    );
    pm.ProcessFill(*fill_long);

    // 2. Sell 2 @ 4010 (Close 1, Open Short 1)
    // PnL on first 1: $500 Profit.
    // New Short Position: -1 from 4010.
    auto flip_short = std::make_unique<StrategyFillEvent>(
        100,
        1,
        kFutInstrumentId,
        OrderSide::kAsk,
        4010'000'000'000,
        2,
        "TestStrat"                 
    );
    pm.ProcessFill(*flip_short);

    EXPECT_EQ(pm.GetPositionQty(kFutInstrumentId), -1);
    EXPECT_EQ(pm.GetPositionByInstrId(kFutInstrumentId).avg_entry_price, 4010'000'000'000);
    EXPECT_EQ(pm.GetRealizedPnL(), 500'000'000'000);
}

// // ==========================================================================
// // MARK: Metrics Tests
// // ==========================================================================

TEST_F(PortfolioManagerTest, Metrics_UnrealizedPnL_Futures) {
    PortfolioManager pm(config_fut_);

    // Open Long 2 @ 4000
    auto fill_long = std::make_unique<StrategyFillEvent>(
        100,
        1,
        kFutInstrumentId,
        OrderSide::kBid,
        4000'000'000'000,
        2,
        "TestStrat"                 
    );
    pm.ProcessFill(*fill_long);

    // Market moves to 4002.
    // Diff 2.0 -> 8 ticks -> 8 * 12.5 = 100 per contract.
    // 2 contracts -> $200 Unrealized.
    Position pos = pm.GetPositionByInstrId(kFutInstrumentId);
    BidAskPair ba_pair = {4002'000'000'000, 1, 1, 4003'000'000'000, 1, 1};
    int64_t upnl = pm.GetUnrealizedPnL(pos, ba_pair);
    EXPECT_EQ(upnl, 200'000'000'000);

    // Total Equity Check
    double equity = pm.GetTotalEquity({{kFutInstrumentId, ba_pair}});
    EXPECT_EQ(equity, kInitialCash + 200'000'000'000);
}

TEST_F(PortfolioManagerTest, Metrics_Drawdown_PreventsTrading) {
    PortfolioManager pm(config_fut_);
    
    // 1. Open massive position (10 contracts) @ 4000
    auto fill_long = std::make_unique<StrategyFillEvent>(
        100,
        1,
        kFutInstrumentId,
        OrderSide::kBid,
        4000'000'000'000,
        10,
        "TestStrat"                 
    );
    pm.ProcessFill(*fill_long);
    
    // 2. Market Crashes to 3800 (200 pts down)
    // Loss per contract: 200 * 50 = 10,000.
    // Total Loss: 100,000.
    // Equity = 100k - 100k = 0.
    // Drawdown = 100%.
    BidAskPair ba_pair = {3799'000'000'000, 1, 1, 3800'000'000'000, 1, 1};
    
    int64_t equity = pm.GetTotalEquity({{kFutInstrumentId, ba_pair}});
    int64_t dd = pm.GetCurrentDrawdown(equity);
    
    EXPECT_NEAR(dd, 1, 1); // 100% drawdown

    // 3. Try to open new trade
    auto signal = CreateSignal(1000, 5, 1, SignalType::kBuySignal, 3800'000'000'000, 50);
    auto order = pm.RequestOrder(&signal, {{kFutInstrumentId, ba_pair}});

    EXPECT_EQ(order, nullptr) << "Should reject order due to max drawdown violation";
}

TEST_F(PortfolioManagerTest, Metrics_BuyingPower_Dynamic) {
    PortfolioManager pm(config_fut_);
    BidAskPair ba_pair = {4000'000'000'000, 1, 1, 4001'000'000'000, 1, 1};
    // Initial BP = 100k
    EXPECT_EQ(pm.GetBuyingPowerAvailable({{kFutInstrumentId, ba_pair}}), 100000'000'000'000);

    // Buy 1 @ 4000 (Margin 5k)
    auto fill_long = std::make_unique<StrategyFillEvent>(
        100,
        1,
        kFutInstrumentId,
        OrderSide::kBid,
        4000'000'000'000,
        1,
        "TestStrat"                 
    );
    pm.ProcessFill(*fill_long);
    
    // BP = 100k - 5k = 95k
    EXPECT_EQ(pm.GetBuyingPowerAvailable({{kFutInstrumentId, ba_pair}}), 95000'000'000'000);
    
    // Price drops, causing Unrealized Loss of $2000
    // Equity = 98k. Margin Used = 5k.
    // BP = 98k - 5k = 93k.
    // 4000 - 40pts = 3960. (40 * 50 = 2000 loss)
    BidAskPair ba_bad = {3960'000'000'000, 1, 1, 3961'000'000'000, 1, 1};
    EXPECT_EQ(pm.GetBuyingPowerAvailable({{kFutInstrumentId, ba_bad}}), 93000'000'000'000);
}

} 