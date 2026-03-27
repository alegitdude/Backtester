#pragma once
#include "../core/Event.h"
#include "../core/Types.h"
#include "../portfolio/PortfolioManager.h"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace backtester {

// ==================================================================================
// MARK: Equity Snapshot (Accumulated During Run)
// ==================================================================================

struct EquitySnapshot {
    uint64_t timestamp = 0;
    int64_t equity = 0;
    int64_t cash = 0;
    int64_t realized_pnl = 0;
    int64_t unrealized_pnl = 0;
    int64_t drawdown = 0;
    bool has_open_position = false;
};

struct InstrumentSnapshot {
    uint64_t timestamp = 0;
    uint32_t instrument_id = 0;
    int64_t quantity = 0;
    int64_t avg_entry_price = 0;
    int64_t unrealized_pnl = 0;
};

// ==================================================================================
// MARK: Computed Summary Statistics
// ==================================================================================

struct PerformanceSummary {
    // --- Return Metrics ---
    int64_t initial_capital = 0;
    int64_t final_equity = 0;
    int64_t total_pnl = 0;
    int64_t total_realized_pnl = 0;
    int64_t total_unrealized_pnl = 0;
    int64_t total_commissions = 0;
    double total_return_pct = 0.0;
    double annualized_return_pct = 0.0;

    // --- Risk Metrics ---
    int64_t max_drawdown = 0;
    double max_drawdown_pct = 0.0;
    uint64_t max_drawdown_duration_ns = 0;
    double annualized_volatility = 0.0;
    double downside_deviation = 0.0;

    // --- Risk-Adjusted Returns ---
    double sharpe_ratio = 0.0;
    double sortino_ratio = 0.0;
    double calmar_ratio = 0.0;
    double profit_factor = 0.0;

    // --- Trade Statistics ---
    uint32_t total_trades = 0;
    uint32_t winning_trades = 0;
    uint32_t losing_trades = 0;
    double win_rate_pct = 0.0;
    int64_t avg_win = 0;
    int64_t avg_loss = 0;
    double avg_win_loss_ratio = 0.0;
    int64_t largest_win = 0;
    int64_t largest_loss = 0;
    int64_t avg_pnl_per_trade = 0;
    double avg_trade_duration_ns = 0.0;

    // --- Position & Exposure ---
    int64_t max_position_held = 0;
    int64_t total_volume_traded = 0;
    double time_in_market_pct = 0.0;

    // --- Timing ---
    uint64_t backtest_start_ts = 0;
    uint64_t backtest_end_ts = 0;
    double backtest_duration_days = 0.0;
};

// ==================================================================================
// MARK: ReportGenerator
// ==================================================================================

class ReportGenerator {
 public:
    ReportGenerator(const AppConfig& config);
    ~ReportGenerator() = default;

    // -------------------------------------------------------------------
    // Called from Backtester::RunLoop periodically to record equity state.
    // -------------------------------------------------------------------
    void RecordEquitySnapshot(uint64_t timestamp, int64_t equity, int64_t cash,
        int64_t realized_pnl, int64_t unrealized_pnl, int64_t drawdown);

    // -------------------------------------------------------------------
    // Called once at the end of the backtest. Computes all metrics and
    // writes CSV files to config.report_output_dir.
    // -------------------------------------------------------------------
    void GenerateReport(const PortfolioManager& portfolio);

 private:
    const AppConfig& config_;
    std::vector<EquitySnapshot> equity_curve_;

    // Peak tracking for drawdown duration
    int64_t peak_equity_ = 0;
    uint64_t peak_equity_ts_ = 0;
    uint64_t max_dd_duration_ns_ = 0;

    // -------------------------------------------------------------------
    // Metric Computation
    // -------------------------------------------------------------------
    PerformanceSummary ComputeSummary(const PortfolioManager& portfolio) const;

    std::unordered_map<std::string, PerformanceSummary> ComputePerStrategySummary(
        const PortfolioManager& portfolio) const;

    PerformanceSummary ComputeSummaryFromTrades(
        const std::vector<TradeRecord>& trades,
        int64_t initial_capital,
        int64_t final_equity) const;

    // -------------------------------------------------------------------
    // Volatility & Risk Helpers
    // -------------------------------------------------------------------
    double ComputeAnnualizedVolatility() const;
    double ComputeDownsideDeviation() const;
    std::vector<double> ComputeReturns() const;

    // -------------------------------------------------------------------
    // CSV Output
    // -------------------------------------------------------------------
    void WriteSummaryCsv(const std::string& filepath,
        const PerformanceSummary& summary) const;

    void WriteTradeLogCsv(const std::string& filepath,
        const std::vector<TradeRecord>& trades) const;

    void WriteEquityCurveCsv(const std::string& filepath) const;

    void WritePerStrategyBreakdownCsv(const std::string& filepath,
        const std::unordered_map<std::string, PerformanceSummary>& breakdowns) const;

    // -------------------------------------------------------------------
    // Formatting Helpers
    // -------------------------------------------------------------------
    static std::string SideToString(OrderSide side);
    static std::string FormatScaledPrice(int64_t price);
};

}