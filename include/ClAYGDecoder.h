//
// Created by tommasopeduzzi on 1/28/24.
//

#ifndef CLAYG_CLAYGDECODER_H
#define CLAYG_CLAYGDECODER_H

#include <utility>

#include "UnionFindDecoder.h"
#include "Decoder.h"

// ClAYGDecoder inherits from UnionFind
// It owns the clusters and the graph, but not its constituents
class ClAYGDecoder : public virtual UnionFindDecoder {
protected:
    int growth_rounds_ = 1;
    bool stop_early_ = false;
    int current_round_ = 0;
    double cluster_lifetime_factor_ = 0;
    std::shared_ptr<DecodingGraph> decoding_graph_;
public:
    explicit ClAYGDecoder(std::unordered_map<std::string, std::string> args = {});

    DecodingResult decode(std::shared_ptr<DecodingGraph> graph) override;

    void merge(const std::vector<DecodingGraphEdge::FusionEdge>& fusion_edges) override;

    std::vector<std::shared_ptr<DecodingGraphEdge>> clean(const std::shared_ptr<DecodingGraph>& decoding_graph);

    virtual void add(const std::shared_ptr<DecodingGraph>& graph, std::shared_ptr<DecodingGraphNode> node);

    void set_growth_rounds(const int growth_rounds) { growth_rounds_ = growth_rounds; }

    void set_stop_early(const bool stop_early) { stop_early_ = stop_early; }

    void set_cluster_lifetime_factor(const double life_time) { cluster_lifetime_factor_ = life_time; }
};

class SingleLayerClAYGDecoder : public virtual ClAYGDecoder
{
public:
    explicit SingleLayerClAYGDecoder(std::unordered_map<std::string, std::string> args = {})
        : ClAYGDecoder(std::move(args))
    {
        decoder_name_ = "sl_" + decoder_name_;
        decoding_graph_ = DecodingGraph::rotated_surface_code(2, 2);
    }

    DecodingResult decode(std::shared_ptr<DecodingGraph> graph) override;

    void add(const std::shared_ptr<DecodingGraph>& graph, std::shared_ptr<DecodingGraphNode> node) override;
};

#endif //CLAYG_CLAYGDECODER_H
