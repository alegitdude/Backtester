#pragma once
#include "IMarketDataProvider.h"
#include "../core/Event.h"
#include "InstrumentState.h"
#include "OrderBook.h"
#include <iostream>

namespace backtester {

class MarketStateManager : public IMarketDataProvider{
 public:
	MarketStateManager() = default;

    void Initialize(const std::vector<uint32_t>& active_ids, 
        std::vector<uint32_t> traded_instr_id);

    void OnMarketEvent(const MarketByOrderEvent& event) ; 
    
    const BidAskPair GetInstrumentBbo(uint32_t instr_id) const;
    std::unordered_map<uint32_t, BidAskPair> GetTradedInstrsBbo();
    
    const std::vector<BidAskPair> GetOBSnapshot(
        uint32_t instrument_id, uint16_t publisher_id, 
        std::size_t level_count) const override ;
    
    const int64_t GetQueueDepth(uint32_t instr_id, int64_t price) const;

    const std::unordered_map<uint32_t, MarketSnapshot>& GetMarketSnapshots() const override { 
        return snapshots_;
    }
 private:
    std::vector<uint32_t> traded_instr_ids_;
    std::vector<InstrumentState> instrument_store_;
    std::vector<InstrumentState*> lookup_table_;

    std::unordered_map<uint32_t, InstrumentState> surprise_instruments_;
    
    std::unordered_map<uint32_t, MarketSnapshot> snapshots_;

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

        return nullptr; 
    }

    void UpdateSnapshot(uint32_t instrument_id, InstrumentState* state) {
        snapshots_[instrument_id] = state->GetMarketSnapshot();
    }
};

}

