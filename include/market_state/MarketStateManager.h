#pragma once
#include "IMarketDataProvider.h"
#include "../core/Event.h"
#include "InstrumentState.h"
#include "OrderBook.h"
#include "spdlog/spdlog.h"

namespace backtester {

    class MarketStateManager : public IMarketDataProvider {
    public:
        MarketStateManager() = default;

        void Initialize(const std::vector<uint32_t>& active_ids);

        void OnMarketEvent(const MarketByOrderEvent& event);

        const BidAskPair GetInstrumentBbo(uint32_t instr_id) const;
        std::unordered_map<uint32_t, BidAskPair> GetTradedInstrsBbo();

        // IMarketDataProvider methods
        const std::vector<BidAskPair> GetOBSnapshotByPub(
            uint32_t instrument_id, uint16_t publisher_id,
            std::size_t level_count) const override;

        int64_t GetQueueDepth(uint32_t instr_id, OrderSide side, int64_t price) const override;

        void GetAggOBBidsSnapshot(uint32_t instrument_id, std::span<PriceLevel> levels) const override;
        void GetAggOBAsksSnapshot(uint32_t instrument_id, std::span<PriceLevel> levels) const override;

        const std::unordered_map<uint32_t, const MarketSnapshot*>& GetMarketSnapshots() const override {
            return snapshots_;
        }

        inline const MarketSnapshot* GetSnapshotByInstr(uint32_t instr_id) const override {
            auto it = snapshots_.find(instr_id);
            if (BT_UNLIKELY(it == snapshots_.end())) {
                throw std::runtime_error(fmt::format("GetSnapshotByInstr in market state "
                    "tried to find an unknown instrument: {}", instr_id));
            }
            return it->second;
        }

    private:
        std::vector<InstrumentState> instrument_store_;
        std::vector<InstrumentState*> lookup_table_;

        std::unordered_map<uint32_t, InstrumentState> surprise_instruments_;

        std::unordered_map<uint32_t, const MarketSnapshot*> snapshots_;

        inline InstrumentState* GetOrCreateInstrumentState(uint32_t id) {
            if (id < lookup_table_.size() && lookup_table_[id]) {
                return lookup_table_[id];
            }
            auto [it, inserted] = surprise_instruments_.try_emplace(id, id);
            // TODO if INSERTED should be logged
            return &it->second;
        }

        const inline InstrumentState* GetInstrumentState(uint32_t id) const {
            if (id < lookup_table_.size() && lookup_table_[id]) {
                return lookup_table_[id];
            }

            auto it = surprise_instruments_.find(id);
            if (it != surprise_instruments_.end()) {
                return &it->second;
            }
            spdlog::error("Tried to access unknown instrument with id: {}", id);
            return nullptr;
        }
    };

}

