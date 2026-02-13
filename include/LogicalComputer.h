#ifndef CLAYG_LOGICALCOMPUTER_H
#define CLAYG_LOGICALCOMPUTER_H

#include <memory>
#include <vector>
#include <unordered_map>
#include <deque>
#include <cstdint>

#include "DecodingGraph.h"

struct DecodingResult;

class LogicalComputer {
public:
    LogicalComputer(const std::shared_ptr<DecodingGraph>& graph);

    // Clear cache when bulk errors / corrections change
    void clear_cache();

    int compute(
        const std::vector<std::shared_ptr<DecodingGraphEdge>>& bulk_errors,
        const std::vector<std::shared_ptr<DecodingGraphEdge>>& idling_errors,
        const DecodingResult& decoding_result
    );

private:
    int num_edges_;
    int num_nodes_;

    // topology caches
    std::vector<std::vector<int>> node_edge_ids_;
    std::set<int> logical_edge_ids_;

    // parity buffer
    std::vector<uint8_t> final_measurement_;

    // node lists
    std::vector<std::shared_ptr<DecodingGraphNode>> nodes_;
    std::vector<std::shared_ptr<DecodingGraphNode>> scratch_graph_nodes_;

    std::shared_ptr<DecodingGraph> scratch_graph_;

    // ---------- idling cache ----------
    uint64_t hash_idling(
        const std::vector<std::shared_ptr<DecodingGraphEdge>>& idling_errors) const;

    std::unordered_map<uint64_t, int> cache_;
    std::deque<uint64_t> cache_fifo_;

    static constexpr size_t MAX_CACHE = 10000;
};

#endif // CLAYG_LOGICALCOMPUTER_H
