#pragma once
#include "../core/Event.h"
#include "InstrumentState.h"
#include "OrderBook.h"
#include <iostream>

namespace backtester {

class MarketStateManager{
 public:
	MarketStateManager() = default;

    void Initialize(const std::vector<uint32_t>& active_ids, 
        std::vector<uint32_t> traded_instr_id);

    void OnMarketEvent(const MarketByOrderEvent& event) ; 

    std::unordered_map<uint32_t, Bbo> GetTradedInstrsBbo();
    
    const std::vector<BidAskPair> GetOBSnapshot(
        uint32_t instrument_id, uint16_t publisher_id, 
        std::size_t level_count = 1) ;

 private:
    std::vector<uint32_t> traded_instr_ids_;
    std::vector<InstrumentState> instrument_store_;
    std::vector<InstrumentState*> lookup_table_;

    std::unordered_map<uint32_t, InstrumentState> surprise_instruments_;
    
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
};

}

