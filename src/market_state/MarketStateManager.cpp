#include "../../include/market_state/MarketStateManager.h"

namespace backtester {

void MarketStateManager::Initialize(const std::vector<uint32_t>& active_ids, 
    std::vector<uint32_t> traded_instr_ids) {
        traded_instr_ids_ = traded_instr_ids;
        // 1. Reserve memory to prevent re-allocation/pointer invalidation
        instrument_store_.reserve(active_ids.size());
        
        // 2. Determine max ID to size the lookup table
        uint32_t max_id = 0;
        for(auto id : active_ids) if(id > max_id) max_id = id;

        // 3. Resize lookup table (filled with nullptr initially)
        // This vector maps: instrument_id -> pointer to State
        lookup_table_.resize(max_id + 1, nullptr);

        // 4. Create the states and populate the lookup table
        for (uint32_t id : active_ids) {
            // Create the state object in the contiguous storage
            instrument_store_.emplace_back(id); 
            
            // Point the lookup slot to this new object
            lookup_table_[id] = &instrument_store_.back();
        }
    }

void MarketStateManager::OnMarketEvent(const MarketByOrderEvent& event) {
        InstrumentState* state = nullptr;
        state = GetOrCreateInstrumentState(event.instrument_id);

        state->OnMarketEvent(event);
}

std::unordered_map<uint32_t, Bbo> MarketStateManager::GetTradedInstrsBbo(){
    std::unordered_map<uint32_t, Bbo> res;
    for(auto instr_state : instrument_store_){
        res[instr_state.instrument_id] = instr_state.GetInstrumentBbo();
    }
    return res;
}

const std::vector<BidAskPair> MarketStateManager::GetOBSnapshot(
        uint32_t instrument_id, uint16_t publisher_id, 
        std::size_t level_count) {
    static const std::vector<BidAskPair> EMPTY_SNAPSHOT;
    
    const InstrumentState* instrument = GetInstrumentState(instrument_id);
    return instrument ? instrument->GetOBSnapshotByPub(publisher_id, level_count) : EMPTY_SNAPSHOT;
}

}