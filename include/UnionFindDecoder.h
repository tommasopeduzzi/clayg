//
// Created by tommasopeduzzi on 1/21/24.
//

#ifndef CLAYG_UNIONFINDDECODER_H
#define CLAYG_UNIONFINDDECODER_H


#include <functional>

#include "DecodingGraph.h"
#include "Decoder.h"

class UnionFindDecoder : public Decoder
{
protected:
    std::vector<std::shared_ptr<Cluster>> m_clusters;
    double last_growth_steps_ = 0;
    std::function<float(DecodingGraphNode::Id, DecodingGraphNode::Id)> growth_policy_ =
        [](DecodingGraphNode::Id start, DecodingGraphNode::Id end)
        {
            return 0.5;
        };
    bool stop_early_ = false;


public:
    explicit UnionFindDecoder(const std::unordered_map<std::string, std::string>& args = {});

    DecodingResult decode(std::shared_ptr<DecodingGraph> graph) override;

    std::vector<DecodingGraphEdge::FusionEdge> grow(const std::shared_ptr<Cluster>& cluster);

    virtual void merge(const std::vector<DecodingGraphEdge::FusionEdge>& fusion_edges);

    int get_last_growth_steps() const override { return last_growth_steps_; }

    void set_growth_policy(const std::function<float(DecodingGraphNode::Id, DecodingGraphNode::Id)> policy) { growth_policy_ = policy; }

    void set_stop_early(const bool stop_early) { stop_early_ = stop_early; }
};


#endif //CLAYG_UNIONFINDDECODER_H
