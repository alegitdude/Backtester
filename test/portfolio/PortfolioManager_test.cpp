#include "../../include/portfolio/PortfolioManager.h"
#include "../../include/core/Types.h"
#include "../../include/core/Event.h"
#include <gtest/gtest.h>
#include <memory>
#include <vector>

namespace backtester {

struct MockFillEvent : public FillEvent {
    MockFillEvent(uint64_t ts, uint32_t instr_id, OrderSide s, int64_t price, uint32_t qty, uint64_t oid) 
    : FillEvent(ts, EventType::kMarketFill) {
        this->instrument_id = instr_id;
        this->side = s;
        this->fill_price = price;
        this->fill_quantity = qty;
        this->order_id = oid;
    }
};

class PortfolioManagerTest : public ::testing::Test {
 protected:
    const uint32_t kInstrumentId = 1;
    const std::string kSymbolStr = "ESZ5";
    const double kInitialCash = 100000.0;
    
    // Config objects
    AppConfig config_fut_;
    AppConfig config_stk_;

    void SetUp() override {
        // 1. Setup Futures Config (ES-like)
        config_fut_.initial_cash = kInitialCash;
        config_fut_.traded_instrument = {
            .instrument_id = kInstrumentId,
            .instrument_type = InstrumentType::FUT,
            .tick_size = 0.25,
            .tick_value = 12.50, // $50 per point (4 ticks)
            .margin_req = 5000.0 // Margin per contract
        };
        config_fut_.risk_limits.max_position_size = 10;
        config_fut_.risk_limits.max_drawdown_pct = 0.10; 
        config_fut_.risk_limits.max_risk_per_trade_pct = 0.02;

        // 2. Setup Stock Config (AAPL-like)
        config_stk_.initial_cash = kInitialCash;
        config_stk_.traded_instrument = {
            .instrument_id = 2,
            .instrument_type = InstrumentType::STOCK,
            .tick_size = 0.01,
            .tick_value = 0.01, // 1:1 value
            .margin_req = 0.0   // Cash account 
        };
    }

    // Helper to create a signal
    StrategySignalEvent CreateSignal(SignalType type, int64_t price, uint32_t qty) {
        return StrategySignalEvent(
            "TestStrat",
            EventType::kStrategySignal,
            1000,           // timestamp
            kInstrumentId,  // symbol/id
            type,
            price,
            qty
        );
    }
};

// ==================================================================================
// MARK: Initialization Tests
// ==================================================================================

TEST_F(PortfolioManagerTest, InitializationState) {
    PortfolioManager pm(kInitialCash, config_fut_);

    EXPECT_DOUBLE_EQ(pm.GetCash(), kInitialCash);
    EXPECT_DOUBLE_EQ(pm.GetRealizedPnL(), 0);
    EXPECT_FALSE(pm.HasPosition(kInstrumentId));
    EXPECT_EQ(pm.GetPositionQty(kInstrumentId), 0);
    // Initial buying power should be Initial Cash (no margin used)
    //EXPECT_DOUBLE_EQ(pm.GetBuyingPowerAvailable({{kInstrumentId, 4000}}), kInitialCash);
    EXPECT_DOUBLE_EQ(pm.GetBuyingPowerAvailable(), kInitialCash);
}

// ==================================================================================
// MARK: Risk Gate Tests (RequestOrder)
// ==================================================================================

TEST_F(PortfolioManagerTest, RiskGate_ValidEntry) {
    PortfolioManager pm(kInitialCash, config_fut_);
    
    // Signal: Buy 1 @ 4000.00 (16000 ticks)
    // 4000.00 / 0.25 = 16000
    int64_t price = 400000; // Assuming input is scaled or raw. Let's assume raw double logic 4000.00
    // Note: The PM implementation uses static_cast<double>(price), so we pass raw integer 4000. 
    // If your system uses fixed point (e.g. 4000000000), adjust here. 
    // Based on ConfigParser test, tick_size is 0.25. 
    // Let's use 4000.00 as 4000 for simplicity if IsValidTick handles it, 
    // OR if we treat price as scaled. The implementation used `std::abs(num_ticks - round) < epsilon`.
    // Let's use 4000.
    
    auto signal = CreateSignal(SignalType::kBuySignal, 4000, 1);
    std::unordered_map<uint32_t, int64_t> market_prices = {{kInstrumentId, 4000}};

    auto order = pm.RequestOrder(&signal, market_prices);

    ASSERT_NE(order, nullptr);
    EXPECT_EQ(order->type, EventType::kStrategyOrderSubmit);
    EXPECT_EQ(order->get_side(), OrderSide::kBid);
    EXPECT_DOUBLE_EQ(order->get_quantity(), 1);
}

TEST_F(PortfolioManagerTest, RiskGate_RejectsInvalidTick) {
    PortfolioManager pm(kInitialCash, config_fut_);
    
    // Tick size is 0.25. Price 4000.10 is NOT valid.
    // 4000.10 / 0.25 = 16000.4 (remainder)
    // We treat inputs as double-compatible logic for this test case
    // We might need to cast to int64 representation of 4000.10 if strictly integer based,
    // but the logic `double num_ticks = price / tick_size` handles float inputs masquerading as ints.
    
    // Let's assume the int64_t input represents the price directly (e.g. 4000 means 4000.0)
    // To represent 4000.10, we'd need a scaler. If the system is raw doubles cast to int:
    // This test depends on how `IsValidTick` interprets the int64.
    // If int64 is "Price * 1e9", then 4000*1e9 is valid.
    // Use a simpler approach: Price 4001 (valid) vs 4001 + 0.1 (impossible in int).
    // Let's assume the int passed IS the price. 
    // 4000 is valid (divisible by 0.25). 
    // 4000 fails if we interpret it as 4000e-9. 
    
    // **Assumed Logic**: Input is simple number.
    // 4000 is valid. 
    // If we pass 4000, it works.
    // We can't pass 4000.1 as int64_t.
    // So `IsValidTick` mainly protects against Strategy logic errors calculating weird levels.
    // Let's simulate a strategy passing a weird integer that isn't a multiple.
    // E.g., if tick is 5, and we pass 6.
    
    AppConfig weird_tick = config_fut_;
    weird_tick.traded_instrument.tick_size = 5.0; 
    PortfolioManager pm_weird(kInitialCash, weird_tick);
    
    auto signal = CreateSignal(SignalType::kBuySignal, 4003, 1); // 4003 not div by 5
    auto order = pm_weird.RequestOrder(&signal, {{kInstrumentId, 4000}});
    
    EXPECT_EQ(order, nullptr) << "Should reject price 4003 when tick size is 5";
}

TEST_F(PortfolioManagerTest, RiskGate_InsufficientBuyingPower) {
    PortfolioManager pm(kInitialCash, config_fut_);
    
    // Cash 100k. Margin 5k. Max contracts ~20.
    // Try to buy 50.
    auto signal = CreateSignal(SignalType::kBuySignal, 4000, 50);
    auto order = pm.RequestOrder(&signal, {{kInstrumentId, 4000}});

    EXPECT_EQ(order, nullptr) << "Should reject order requiring 250k margin with only 100k cash";
}

TEST_F(PortfolioManagerTest, RiskGate_PositionLimits) {
    PortfolioManager pm(kInitialCash, config_fut_);
    // Limit is 10.
    
    // 1. Fill a position of 10.
    MockFillEvent fill(100, kInstrumentId, OrderSide::kBid, 4000, 10, 1);
    pm.ProcessFill(fill);
    
    // 2. Try to buy 1 more.
    auto signal = CreateSignal(SignalType::kBuySignal, 4000, 1);
    auto order = pm.RequestOrder(&signal, {{kInstrumentId, 4000}});

    EXPECT_EQ(order, nullptr) << "Should reject order exceeding max position limit";
}

// ==================================================================================
// MARK: Execution & PnL Tests (Futures)
// ==================================================================================

TEST_F(PortfolioManagerTest, Execution_OpenAndClose_Profit) {
    PortfolioManager pm(kInitialCash, config_fut_);

    // 1. Open Long 1 @ 4000
    MockFillEvent fill_open(100, kInstrumentId, OrderSide::kBid, 4000, 1, 1);
    pm.ProcessFill(fill_open);

    EXPECT_EQ(pm.GetPositionQuantity(kInstrumentId), 1);
    EXPECT_DOUBLE_EQ(pm.GetCash(), kInitialCash); // Cash doesn't change on open in this model

    // 2. Close Long 1 @ 4010
    // Profit calc: (4010 - 4000) = 10 points.
    // Tick size 0.25 -> 40 ticks.
    // Tick val 12.50 -> 40 * 12.50 = $500 Profit.
    MockFillEvent fill_close(200, kInstrumentId, OrderSide::kAsk, 4010, 1, 2);
    pm.ProcessFill(fill_close);

    EXPECT_EQ(pm.GetPositionQuantity(kInstrumentId), 0);
    EXPECT_DOUBLE_EQ(pm.GetRealizedPnL(), 500.0);
    EXPECT_DOUBLE_EQ(pm.GetCash(), kInitialCash + 500.0);
}

TEST_F(PortfolioManagerTest, Execution_OpenAndClose_Loss) {
    PortfolioManager pm(kInitialCash, config_fut_);

    // 1. Open Short 1 @ 4000
    MockFillEvent fill_open(100, kInstrumentId, OrderSide::kAsk, 4000, 1, 1);
    pm.ProcessFill(fill_open);

    EXPECT_EQ(pm.GetPositionQuantity(kInstrumentId), -1);

    // 2. Close Short 1 @ 4005 (Market went up, we lose)
    // Loss: 5 points * (1/0.25) * 12.5 = 20 * 12.5 = $250 Loss.
    MockFillEvent fill_close(200, kInstrumentId, OrderSide::kBid, 4005, 1, 2);
    pm.ProcessFill(fill_close);

    EXPECT_EQ(pm.GetPositionQuantity(kInstrumentId), 0);
    EXPECT_DOUBLE_EQ(pm.GetRealizedPnL(), -250.0);
    EXPECT_DOUBLE_EQ(pm.GetCash(), kInitialCash - 250.0);
}

TEST_F(PortfolioManagerTest, Execution_PositionFlip) {
    PortfolioManager pm(kInitialCash, config_fut_);

    // 1. Open Long 1 @ 4000
    pm.ProcessFill(MockFillEvent(100, kInstrumentId, OrderSide::kBid, 4000, 1, 1));

    // 2. Sell 2 @ 4010 (Close 1, Open Short 1)
    // PnL on first 1: $500 Profit.
    // New Short Position: -1 from 4010.
    pm.ProcessFill(MockFillEvent(200, kInstrumentId, OrderSide::kAsk, 4010, 2, 2));

    EXPECT_EQ(pm.GetPositionQuantity(kInstrumentId), -1);
    EXPECT_DOUBLE_EQ(pm.GetPosition(kInstrumentId).average_entry_price, 4010.0);
    EXPECT_DOUBLE_EQ(pm.GetRealizedPnL(), 500.0);
}

// ==================================================================================
// MARK: Metrics Tests
// ==================================================================================

TEST_F(PortfolioManagerTest, Metrics_UnrealizedPnL_Futures) {
    PortfolioManager pm(kInitialCash, config_fut_);

    // Open Long 2 @ 4000
    pm.ProcessFill(MockFillEvent(100, kInstrumentId, OrderSide::kBid, 4000, 2, 1));

    // Market moves to 4002.
    // Diff 2.0 -> 8 ticks -> 8 * 12.5 = 100 per contract.
    // 2 contracts -> $200 Unrealized.
    double upnl = pm.GetUnrealizedPnL(kInstrumentId, 4002);
    EXPECT_DOUBLE_EQ(upnl, 200.0);

    // Total Equity Check
    double equity = pm.GetTotalEquity({{kInstrumentId, 4002}});
    EXPECT_DOUBLE_EQ(equity, kInitialCash + 200.0);
}

TEST_F(PortfolioManagerTest, Metrics_Drawdown_PreventsTrading) {
    PortfolioManager pm(kInitialCash, config_fut_);
    
    // 1. Open massive position (10 contracts) @ 4000
    pm.ProcessFill(MockFillEvent(100, kInstrumentId, OrderSide::kBid, 4000, 10, 1));
    
    // 2. Market Crashes to 3800 (200 pts down)
    // Loss per contract: 200 * 50 = 10,000.
    // Total Loss: 100,000.
    // Equity = 100k - 100k = 0.
    // Drawdown = 100%.
    
    std::unordered_map<uint32_t, int64_t> crash_prices = {{kInstrumentId, 3800}};
    
    double equity = pm.GetTotalEquity(crash_prices);
    double dd = pm.GetCurrentDrawdown(equity);
    
    EXPECT_NEAR(dd, 1.0, 0.001); // 100% drawdown

    // 3. Try to open new trade
    auto signal = CreateSignal(SignalType::kBuySignal, 3800, 1);
    auto order = pm.RequestOrder(&signal, crash_prices);

    EXPECT_EQ(order, nullptr) << "Should reject order due to max drawdown violation";
}

TEST_F(PortfolioManagerTest, Metrics_BuyingPower_Dynamic) {
    PortfolioManager pm(kInitialCash, config_fut_);
    
    // Initial BP = 100k
    EXPECT_DOUBLE_EQ(pm.GetBuyingPowerAvailable({{kInstrumentId, 4000}}), 100000.0);

    // Buy 1 @ 4000 (Margin 5k)
    pm.ProcessFill(MockFillEvent(100, kInstrumentId, OrderSide::kBid, 4000, 1, 1));
    
    // BP = 100k - 5k = 95k
    EXPECT_DOUBLE_EQ(pm.GetBuyingPowerAvailable({{kInstrumentId, 4000}}), 95000.0);
    
    // Price drops, causing Unrealized Loss of $2000
    // Equity = 98k. Margin Used = 5k.
    // BP = 98k - 5k = 93k.
    // 4000 - 40pts = 3960. (40 * 50 = 2000 loss)
    EXPECT_DOUBLE_EQ(pm.GetBuyingPowerAvailable({{kInstrumentId, 3960}}), 93000.0);
}

} 