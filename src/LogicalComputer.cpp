#include "LogicalComputer.h"

#include <algorithm>

#include "UnionFindDecoder.h"
#include "Decoder.h"

// ------------------------------------------------------------
// Constructor
// ------------------------------------------------------------

LogicalComputer::LogicalComputer(const std::shared_ptr<DecodingGraph>& graph)
{
    // cache logical edges
    logical_edge_ids_ = graph->logical_edge_ids();

    // create reusable single-layer graph
    scratch_graph_ = DecodingGraph::single_layer_copy(graph);
    scratch_graph_nodes_ = scratch_graph_->nodes();

    num_edges_ = static_cast<int>(scratch_graph_->edges().size());
    num_nodes_ = static_cast<int>(scratch_graph_->nodes().size());

    final_measurement_.resize(num_edges_);

    // cache node -> edge ids
    node_edge_ids_.resize(num_nodes_);

    for (int i = 0; i < num_nodes_; ++i) {
        for (auto& w : scratch_graph_nodes_[i]->edges()) {
            auto e = w.lock();
            node_edge_ids_[i].push_back(e->id().id);
        }
    }

    cache_.reserve(MAX_CACHE);
}

void LogicalComputer::clear_cache()
{
    cache_.clear();
    cache_fifo_.clear();
}

uint64_t LogicalComputer::hash_idling(
    const std::vector<std::shared_ptr<DecodingGraphEdge>>& idling_errors) const
{
    uint64_t h = 1469598103934665603ULL;

    for (auto& e : idling_errors) {
        uint64_t x = static_cast<uint64_t>(e->id().id);
        h ^= x + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    }

    return h;
}

int LogicalComputer::compute(
    const std::vector<std::shared_ptr<DecodingGraphEdge>>& bulk_errors,
    const std::vector<std::shared_ptr<DecodingGraphEdge>>& idling_errors,
    const DecodingResult& decoding_result)
{
    // Check if this set of idling errors has already been computed
    uint64_t key = hash_idling(idling_errors);
    auto it = cache_.find(key);
    if (it != cache_.end())
        return it->second;

    // Reset final measurements
    std::fill(final_measurement_.begin(),
              final_measurement_.end(), 0);

    // Apply errors up to considered round, corrections, and idling errors
    int consider = decoding_result.considered_up_to_round;
    auto apply = [&](const auto& vec)
    {
        for (auto& e : vec) {
            if (e->id().round <= consider)
                final_measurement_[e->id().id] ^= 1;
        }
    };

    apply(bulk_errors);
    apply(decoding_result.corrections);
    apply(idling_errors);

    // Compute final classical syndrome on the single-layer graph
    scratch_graph_->reset();
    for (int i = 0; i < num_nodes_; ++i) {
        bool defect = false;
        for (int eid : node_edge_ids_[i])
            defect ^= final_measurement_[eid];

        scratch_graph_nodes_[i]->set_marked(defect);
    }

    // Do final classical decoding step
    UnionFindDecoder uf;
    auto classical = uf.decode(scratch_graph_);

    for (auto& e : classical.corrections)
        final_measurement_[e->id().id] ^= 1;

    // Compute logical parity of logical
    uint8_t logical = 0;
    for (int eid : logical_edge_ids_)
        logical ^= final_measurement_[eid];

    int result = logical;

    // Cache result
    if (cache_.size() >= MAX_CACHE) {
        uint64_t old = cache_fifo_.front();
        cache_fifo_.pop_front();
        cache_.erase(old);
    }

    cache_[key] = result;
    cache_fifo_.push_back(key);

    return result;
}
