#include "../../include/reporting/ReportGenerator.h"
#include "../../include/utils/TimeUtils.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>
#include <filesystem>
#include <sstream>
#include <iomanip>

namespace backtester {

    static constexpr double kNanosPerDay = 86400.0 * 1e9;
    static constexpr double kNanosPerYear = kNanosPerDay * 252.0; // Trading days
    static constexpr double kScaleFactor = 1e9;

    // =============================================================================
    // MARK: Construction
    // =============================================================================

    ReportGenerator::ReportGenerator(const AppConfig& config)
        : config_(config),
        peak_equity_(config.initial_cash) {
        equity_curve_.reserve(100000);
    }

    // =============================================================================
    // MARK: Equity Snapshot Recording
    // =============================================================================

    void ReportGenerator::RecordEquitySnapshot(uint64_t timestamp, int64_t equity,
        int64_t cash, int64_t realized_pnl, int64_t unrealized_pnl, int64_t drawdown,
        bool has_open_position) {

        // Track peak for drawdown duration calculation
        if (equity >= peak_equity_) {
            peak_equity_ = equity;
            peak_equity_ts_ = timestamp;
        }
        else {
            uint64_t dd_duration = timestamp - peak_equity_ts_;
            if (dd_duration > max_dd_duration_ns_) {
                max_dd_duration_ns_ = dd_duration;
            }
        }

        equity_curve_.push_back({
            timestamp, equity, cash, realized_pnl,
            unrealized_pnl, drawdown, has_open_position
            });
    }

    // =============================================================================
    // MARK: Report Generation 
    // =============================================================================

    void ReportGenerator::GenerateReport(const PortfolioManager& portfolio) {
        spdlog::info("ReportGenerator: Computing performance metrics...");

        std::string output_dir = config_.report_output_dir;
        if (!output_dir.empty()) {
            std::filesystem::create_directories(output_dir);
        }

        // 1. Overall summary
        PerformanceSummary summary = ComputeSummary(portfolio);
        WriteSummaryCsv(output_dir + "/summary.csv", summary);

        // 2. Trade log
        WriteTradeLogCsv(output_dir + "/trade_log.csv", portfolio.GetTradeHistory());

        // 3. Equity curve
        WriteEquityCurveCsv(output_dir + "/equity_curve.csv");

        // 4. Per-strategy breakdown
        auto breakdowns = ComputePerStrategySummary(portfolio);
        if (breakdowns.size() > 1) {
            WritePerStrategyBreakdownCsv(output_dir + "/strategy_breakdown.csv",
                breakdowns);
        }

        spdlog::info("ReportGenerator: Reports written to {}", output_dir);
        spdlog::info("ReportGenerator: Total PnL={} | Return={:.2f}% | "
            "Sharpe={:.3f} | MaxDD={:.2f}% | Trades={}",
            FormatScaledPrice(summary.total_pnl),
            summary.total_return_pct * 100.0,
            summary.sharpe_ratio,
            summary.max_drawdown_pct * 100.0,
            summary.total_trades);
    }

    // =============================================================================
    // MARK: Summary Computation
    // =============================================================================

    PerformanceSummary ReportGenerator::ComputeSummary(
        const PortfolioManager& portfolio) const {

        const auto& trades = portfolio.GetTradeHistory();
        int64_t initial = config_.initial_cash;

        int64_t final_equity = initial;
        if (!equity_curve_.empty()) {
            final_equity = equity_curve_.back().equity;
        }

        PerformanceSummary summary = ComputeSummaryFromTrades(
            trades, initial, final_equity);

        // Timing
        summary.backtest_start_ts = config_.start_time;
        summary.backtest_end_ts = config_.end_time;
        double duration_ns = static_cast<double>(config_.end_time - config_.start_time);
        summary.backtest_duration_days = duration_ns / kNanosPerDay;

        // Unrealized PnL from last snapshot
        if (!equity_curve_.empty()) {
            summary.total_unrealized_pnl = equity_curve_.back().unrealized_pnl;
        }

        // Volatility and risk-adjusted metrics from equity curve
        summary.annualized_volatility = ComputeAnnualizedVolatility();
        summary.downside_deviation = ComputeDownsideDeviation();
        summary.max_drawdown_duration_ns = max_dd_duration_ns_;

        // Max drawdown from equity curve
        int64_t curve_peak = initial;
        int64_t max_dd = 0;
        for (const auto& snap : equity_curve_) {
            if (snap.equity > curve_peak) curve_peak = snap.equity;
            int64_t dd = curve_peak - snap.equity;
            if (dd > max_dd) max_dd = dd;
        }
        summary.max_drawdown = max_dd;
        summary.max_drawdown_pct = (curve_peak > 0)
            ? static_cast<double>(max_dd) / static_cast<double>(curve_peak)
            : 0.0;

        // CAGR
        double duration_years = summary.backtest_duration_days / 252.0;
        if (duration_years > 0.0 && initial > 0) {
            double equity_ratio = static_cast<double>(final_equity) /
                static_cast<double>(initial);
            if (equity_ratio > 0.0) {
                summary.annualized_return_pct = std::pow(equity_ratio,
                    1.0 / duration_years) - 1.0;
            }
        }

        // Sharpe: annualized_return / annualized_vol
        if (summary.annualized_volatility > 0.0) {
            summary.sharpe_ratio = (summary.annualized_return_pct -
                config_.risk_free_rate) / summary.annualized_volatility;
        }

        // Sortino: annualized_return / downside_deviation
        if (summary.downside_deviation > 0.0) {
            summary.sortino_ratio = (summary.annualized_return_pct
                - config_.risk_free_rate) / summary.downside_deviation;
        }

        // Calmar: annualized_return / max_drawdown_pct
        if (summary.max_drawdown_pct > 0.0) {
            summary.calmar_ratio = summary.annualized_return_pct /
                summary.max_drawdown_pct;
        }

        // Time in market from equity curve
        if (!equity_curve_.empty()) {
            uint64_t total_time = equity_curve_.back().timestamp -
                equity_curve_.front().timestamp;
            uint64_t time_with_position = 0;

            for (size_t i = 1; i < equity_curve_.size(); ++i) {
                if (equity_curve_[i - 1].has_open_position == true) {
                    time_with_position +=
                        equity_curve_[i].timestamp - equity_curve_[i - 1].timestamp;
                }
            }

            summary.time_in_market_pct = (total_time > 0)
                ? static_cast<double>(time_with_position) /
                static_cast<double>(total_time)
                : 0.0;
        }

        return summary;
    }

    // =============================================================================
    // MARK: Compute Summary From Trade List
    // =============================================================================
    // Used for both overall and per-strategy breakdowns.

    PerformanceSummary ReportGenerator::ComputeSummaryFromTrades(
        const std::vector<TradeRecord>& trades,
        int64_t initial_capital,
        int64_t final_equity) const {

        PerformanceSummary s;
        s.initial_capital = initial_capital;
        s.final_equity = final_equity;
        s.total_pnl = final_equity - initial_capital;
        s.total_return_pct = (initial_capital > 0)
            ? static_cast<double>(s.total_pnl) / static_cast<double>(initial_capital)
            : 0.0;

        if (trades.empty()) return s;

        s.total_trades = static_cast<uint32_t>(trades.size());

        int64_t gross_profit = 0;
        int64_t gross_loss = 0;
        int64_t total_win_pnl = 0;
        int64_t total_loss_pnl = 0;
        int64_t max_position = 0;
        int64_t total_volume = 0;

        // Track position per instrument for duration calculation
        // (simplified: uses trade timestamps as proxy)
        uint64_t first_trade_ts = trades.front().timestamp;
        uint64_t last_trade_ts = trades.back().timestamp;

        for (const auto& trade : trades) {
            s.total_commissions += trade.commission;
            total_volume += trade.quantity;

            int64_t abs_qty = std::abs(static_cast<int64_t>(trade.quantity));
            if (abs_qty > max_position) max_position = abs_qty;

            if (trade.realized_pnl > 0) {
                s.winning_trades++;
                gross_profit += trade.realized_pnl;
                total_win_pnl += trade.realized_pnl;
                if (trade.realized_pnl > s.largest_win) {
                    s.largest_win = trade.realized_pnl;
                }
            }
            else if (trade.realized_pnl < 0) {
                s.losing_trades++;
                gross_loss += std::abs(trade.realized_pnl);
                total_loss_pnl += trade.realized_pnl;
                if (trade.realized_pnl < s.largest_loss) {
                    s.largest_loss = trade.realized_pnl;
                }
            }
        }

        s.total_realized_pnl = gross_profit - gross_loss;
        s.max_position_held = max_position;
        s.total_volume_traded = total_volume;

        // Win rate
        uint32_t closed_trades = s.winning_trades + s.losing_trades;
        if (closed_trades > 0) {
            s.win_rate_pct = static_cast<double>(s.winning_trades) / closed_trades;
        }

        // Average win / loss
        if (s.winning_trades > 0) {
            s.avg_win = total_win_pnl / s.winning_trades;
        }
        if (s.losing_trades > 0) {
            s.avg_loss = total_loss_pnl / s.losing_trades; // Negative value
        }

        // Win/loss ratio
        if (s.avg_loss != 0) {
            s.avg_win_loss_ratio = static_cast<double>(s.avg_win) /
                static_cast<double>(std::abs(s.avg_loss));
        }

        // Profit factor
        if (gross_loss > 0) {
            s.profit_factor = static_cast<double>(gross_profit) /
                static_cast<double>(gross_loss);
        }
        else if (gross_profit > 0) {
            s.profit_factor = std::numeric_limits<double>::infinity();
        }
        else {
            s.profit_factor = 0.0; // no trades or all flat
        }

        // Average PnL per trade
        if (s.total_trades > 0) {
            s.avg_pnl_per_trade = s.total_realized_pnl /
                static_cast<int64_t>(s.total_trades);
        }

        // Average trade duration (rough: total span / num trades)
        if (s.total_trades > 1) {
            s.avg_trade_duration_ns = static_cast<double>(
                last_trade_ts - first_trade_ts) / static_cast<double>(s.total_trades);
        }

        return s;
    }

    // =============================================================================
    // MARK: Per-Strategy Breakdown
    // =============================================================================

    std::unordered_map<std::string, PerformanceSummary>
        ReportGenerator::ComputePerStrategySummary(
            const PortfolioManager& portfolio) const {

        const auto& all_trades = portfolio.GetTradeHistory();

        // Group trades by strategy_id
        std::unordered_map<std::string, std::vector<TradeRecord>> by_strategy;
        for (const auto& trade : all_trades) {
            by_strategy[trade.strategy_id].push_back(trade);
        }

        std::unordered_map<std::string, PerformanceSummary> result;
        for (auto& [strategy_id, trades] : by_strategy) {
            // Per-strategy doesn't have its own equity curve, so final_equity
            // is approximated as initial_capital + strategy's realized PnL
            int64_t strat_realized = 0;
            for (const auto& t : trades) strat_realized += t.realized_pnl;

            int64_t strat_equity = config_.initial_cash + strat_realized;
            result[strategy_id] = ComputeSummaryFromTrades(
                trades, config_.initial_cash, strat_equity);
        }

        return result;
    }

    // =============================================================================
    // MARK: Volatility & Risk Helpers
    // =============================================================================

    std::vector<double> ReportGenerator::ComputeReturns() const {
        std::vector<double> returns;
        if (equity_curve_.size() < 2) return returns;

        returns.reserve(equity_curve_.size() - 1);
        for (size_t i = 1; i < equity_curve_.size(); ++i) {
            double prev = static_cast<double>(equity_curve_[i - 1].equity);
            double curr = static_cast<double>(equity_curve_[i].equity);
            if (prev > 0.0) {
                returns.push_back((curr - prev) / prev);
            }
        }
        return returns;
    }

    double ReportGenerator::ComputeAnnualizedVolatility() const {
        auto returns = ComputeReturns();
        if (returns.size() < 2) return 0.0;

        double mean = std::accumulate(returns.begin(), returns.end(), 0.0) /
            static_cast<double>(returns.size());

        double sum_sq = 0.0;
        for (double r : returns) {
            double diff = r - mean;
            sum_sq += diff * diff;
        }

        double variance = sum_sq / static_cast<double>(returns.size() - 1);
        double std_dev = std::sqrt(variance);

        // Annualize: scale by sqrt of number of observations per year
        // We estimate observations per year from the data
        if (equity_curve_.size() < 2) return 0.0;
        double total_ns = static_cast<double>(
            equity_curve_.back().timestamp - equity_curve_.front().timestamp);
        double obs_per_year = (total_ns > 0.0)
            ? static_cast<double>(returns.size()) * (kNanosPerYear / total_ns)
            : 0.0;

        return std_dev * std::sqrt(obs_per_year);
    }

    double ReportGenerator::ComputeDownsideDeviation() const {
        auto returns = ComputeReturns();
        if (returns.size() < 2) return 0.0;

        double sum_sq = 0.0;
        size_t count = 0;
        for (double r : returns) {
            if (r < 0.0) {
                sum_sq += r * r;
                count++;
            }
        }

        if (count == 0) return 0.0;
        double downside_var = sum_sq / static_cast<double>(returns.size()); // Use full N
        double downside_std = std::sqrt(downside_var);

        // Annualize same as volatility
        double total_ns = static_cast<double>(
            equity_curve_.back().timestamp - equity_curve_.front().timestamp);
        double obs_per_year = (total_ns > 0.0)
            ? static_cast<double>(returns.size()) * (kNanosPerYear / total_ns)
            : 0.0;

        return downside_std * std::sqrt(obs_per_year);
    }

    // =============================================================================
    // MARK: CSV Writers
    // =============================================================================

    void ReportGenerator::WriteSummaryCsv(const std::string& filepath,
        const PerformanceSummary& s) const {

        std::ofstream file(filepath);
        if (!file.is_open()) {
            spdlog::error("ReportGenerator: Failed to open {}", filepath);
            return;
        }

        file << "metric,value\n";

        // Return metrics
        file << "initial_capital," << FormatScaledPrice(s.initial_capital) << "\n";
        file << "final_equity," << FormatScaledPrice(s.final_equity) << "\n";
        file << "total_pnl," << FormatScaledPrice(s.total_pnl) << "\n";
        file << "total_realized_pnl," << FormatScaledPrice(s.total_realized_pnl) << "\n";
        file << "total_unrealized_pnl," << FormatScaledPrice(s.total_unrealized_pnl) << "\n";
        file << "total_commissions," << FormatScaledPrice(s.total_commissions) << "\n";
        file << std::fixed << std::setprecision(4);
        file << "total_return_pct," << s.total_return_pct * 100.0 << "\n";
        file << "annualized_return_pct," << s.annualized_return_pct * 100.0 << "\n";

        // Risk metrics
        file << "max_drawdown," << FormatScaledPrice(s.max_drawdown) << "\n";
        file << "max_drawdown_pct," << s.max_drawdown_pct * 100.0 << "\n";
        file << "max_drawdown_duration_days," <<
            static_cast<double>(s.max_drawdown_duration_ns) / kNanosPerDay << "\n";
        file << "annualized_volatility," << s.annualized_volatility * 100.0 << "\n";
        file << "downside_deviation," << s.downside_deviation * 100.0 << "\n";

        // Risk-adjusted
        file << "sharpe_ratio," << s.sharpe_ratio << "\n";
        file << "sortino_ratio," << s.sortino_ratio << "\n";
        file << "calmar_ratio," << s.calmar_ratio << "\n";
        file << "profit_factor," << s.profit_factor << "\n";

        // Trade stats
        file << "total_trades," << s.total_trades << "\n";
        file << "winning_trades," << s.winning_trades << "\n";
        file << "losing_trades," << s.losing_trades << "\n";
        file << "win_rate_pct," << s.win_rate_pct * 100.0 << "\n";
        file << "avg_win," << FormatScaledPrice(s.avg_win) << "\n";
        file << "avg_loss," << FormatScaledPrice(s.avg_loss) << "\n";
        file << "avg_win_loss_ratio," << s.avg_win_loss_ratio << "\n";
        file << "largest_win," << FormatScaledPrice(s.largest_win) << "\n";
        file << "largest_loss," << FormatScaledPrice(s.largest_loss) << "\n";
        file << "avg_pnl_per_trade," << FormatScaledPrice(s.avg_pnl_per_trade) << "\n";

        // Position & exposure
        file << "max_position_held," << s.max_position_held << "\n";
        file << "total_volume_traded," << s.total_volume_traded << "\n";
        file << "time_in_market_pct," << s.time_in_market_pct * 100.0 << "\n";

        // Timing
        file << "backtest_duration_days," << s.backtest_duration_days << "\n";

        file.close();
        spdlog::info("ReportGenerator: Summary written to {}", filepath);
    }

    void ReportGenerator::WriteTradeLogCsv(const std::string& filepath,
        const std::vector<TradeRecord>& trades) const {

        std::ofstream file(filepath);
        if (!file.is_open()) {
            spdlog::error("ReportGenerator: Failed to open {}", filepath);
            return;
        }

        file << "timestamp,strategy_id,instrument_id,side,price,quantity,"
            << "realized_pnl,commission\n";

        for (const auto& t : trades) {
            file << t.timestamp << ","
                << t.strategy_id << ","
                << t.instrument_id << ","
                << SideToString(t.side) << ","
                << FormatScaledPrice(t.price) << ","
                << t.quantity << ","
                << FormatScaledPrice(t.realized_pnl) << ","
                << FormatScaledPrice(t.commission) << "\n";
        }

        file.close();
        spdlog::info("ReportGenerator: Trade log written to {} ({} trades)",
            filepath, trades.size());
    }

    void ReportGenerator::WriteEquityCurveCsv(const std::string& filepath) const {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            spdlog::error("ReportGenerator: Failed to open {}", filepath);
            return;
        }

        file << "timestamp,equity,cash,realized_pnl,unrealized_pnl,"
            << "drawdown,open_position_qty\n";

        for (const auto& snap : equity_curve_) {
            file << snap.timestamp << ","
                << FormatScaledPrice(snap.equity) << ","
                << FormatScaledPrice(snap.cash) << ","
                << FormatScaledPrice(snap.realized_pnl) << ","
                << FormatScaledPrice(snap.unrealized_pnl) << ","
                << FormatScaledPrice(snap.drawdown) << ","
                << snap.has_open_position << "\n";
        }

        file.close();
        spdlog::info("ReportGenerator: Equity curve written to {} ({} snapshots)",
            filepath, equity_curve_.size());
    }

    void ReportGenerator::WritePerStrategyBreakdownCsv(const std::string& filepath,
        const std::unordered_map<std::string, PerformanceSummary>& breakdowns) const {

        std::ofstream file(filepath);
        if (!file.is_open()) {
            spdlog::error("ReportGenerator: Failed to open {}", filepath);
            return;
        }

        file << "strategy_id,total_pnl,total_return_pct,total_trades,winning_trades,"
            << "losing_trades,win_rate_pct,profit_factor,avg_win,avg_loss,"
            << "largest_win,largest_loss,avg_pnl_per_trade,max_position_held,"
            << "total_volume_traded,total_commissions\n";

        file << std::fixed << std::setprecision(4);

        for (const auto& [strategy_id, s] : breakdowns) {
            file << strategy_id << ","
                << FormatScaledPrice(s.total_pnl) << ","
                << s.total_return_pct * 100.0 << ","
                << s.total_trades << ","
                << s.winning_trades << ","
                << s.losing_trades << ","
                << s.win_rate_pct * 100.0 << ","
                << s.profit_factor << ","
                << FormatScaledPrice(s.avg_win) << ","
                << FormatScaledPrice(s.avg_loss) << ","
                << FormatScaledPrice(s.largest_win) << ","
                << FormatScaledPrice(s.largest_loss) << ","
                << FormatScaledPrice(s.avg_pnl_per_trade) << ","
                << s.max_position_held << ","
                << s.total_volume_traded << ","
                << FormatScaledPrice(s.total_commissions) << "\n";
        }

        file.close();
        spdlog::info("ReportGenerator: Strategy breakdown written to {} ({} strategies)",
            filepath, breakdowns.size());
    }

    // =============================================================================
    // MARK: Formatting Helpers
    // =============================================================================

    std::string ReportGenerator::SideToString(OrderSide side) {
        switch (side) {
        case OrderSide::kBid: return "BUY";
        case OrderSide::kAsk: return "SELL";
        default: return "NONE";
        }
    }

    std::string ReportGenerator::FormatScaledPrice(int64_t price) {
        // Convert from 1e9 fixed-point to decimal string
        bool negative = price < 0;
        int64_t abs_price = std::abs(price);
        int64_t whole = abs_price / 1'000'000'000LL;
        int64_t frac = abs_price % 1'000'000'000LL;

        std::ostringstream oss;
        if (negative) oss << "-";
        oss << whole << "." << std::setfill('0') << std::setw(2)
            << (frac / 10'000'000LL); // 2 decimal places
        return oss.str();
    }

}