#include "market_state/MarketStateManager.h"
#include "spdlog/spdlog.h"

namespace backtester {

void MarketStateManager::Initialize(const std::vector<uint32_t>& active_ids) {
        instrument_store_.reserve(active_ids.size());
        
        uint32_t max_id = 0;
        for(auto id : active_ids) if(id > max_id) max_id = id;

        // This vector maps: instrument_id -> pointer to State
        lookup_table_.resize(max_id + 1, nullptr);

        for (uint32_t id : active_ids) {
            instrument_store_.emplace_back(id);          
            lookup_table_[id] = &instrument_store_.back();
            snapshots_[id] = &lookup_table_[id]->GetMarketSnapshot();
        }
    }

void MarketStateManager::OnMarketEvent(const MarketByOrderEvent& event) {
    GetOrCreateInstrumentState(event.instrument_id)->OnMarketEvent(event);
}

const BidAskPair MarketStateManager::GetInstrumentBbo(uint32_t instr_id) const {
    const auto& instr = GetInstrumentState(instr_id);
    if(instr != nullptr){ return instr-> GetInstrumentBbo();}
    spdlog::error("Tried to get instrument bbo for non existent instrument: {}"
        , instr_id);
    return {};
}

std::unordered_map<uint32_t, BidAskPair> MarketStateManager::GetTradedInstrsBbo(){
    std::unordered_map<uint32_t, BidAskPair> res;
    for(const auto& instr_state : instrument_store_){
        res[instr_state.instrument_id] = instr_state.GetInstrumentBbo();
    }
    return res;
}

int64_t MarketStateManager::GetQueueDepth(uint32_t instr_id, OrderSide side, int64_t price) const{
    const InstrumentState* instrument_state = GetInstrumentState(instr_id);
    return instrument_state ? instrument_state->GetQueueDepthByPx(side, price) 
        : kUndefPrice;
}

const std::vector<BidAskPair> MarketStateManager::GetOBSnapshotByPub(
    uint32_t instrument_id, uint16_t publisher_id, 
    std::size_t level_count) const {
        
    static const std::vector<BidAskPair> EMPTY_SNAPSHOT;
    
    const InstrumentState* instrument = GetInstrumentState(instrument_id);
    return instrument ? instrument->GetOBSnapshotByPub(publisher_id, level_count) 
        : EMPTY_SNAPSHOT;
}

void MarketStateManager::GetAggOBBidsSnapshot(uint32_t instrument_id, 
    std::span<PriceLevel> levels) const {
        const InstrumentState* instr = GetInstrumentState(instrument_id);
        if(BT_UNLIKELY (!instr)) throw std::runtime_error(fmt::format("GetAggOBAsks" 
            "tried to access an unknown instrument: {}", instrument_id));
        instr->GetAggOBBidsSnapshot(levels);
} 

void MarketStateManager::GetAggOBAsksSnapshot(uint32_t instrument_id, 
    std::span<PriceLevel> levels) const {
        const InstrumentState* instr = GetInstrumentState(instrument_id);
        if(BT_UNLIKELY (!instr)) throw std::runtime_error(fmt::format("GetAggOBAsks" 
            "tried to access an unknown instrument: {}", instrument_id));
        instr->GetAggOBAsksSnapshot(levels);
} 

}