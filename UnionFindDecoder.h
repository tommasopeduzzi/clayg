//
// Created by tommasopeduzzi on 1/21/24.
//

#ifndef CLAYG_UNIONFINDDECODER_H
#define CLAYG_UNIONFINDDECODER_H


#include <functional>

#include "DecodingGraph.h"
#include "PeelingDecoder.h"
#include "Decoder.h"

class UnionFindDecoder : public virtual Decoder
{
protected:
    std::vector<std::shared_ptr<Cluster>> m_clusters;
    double last_growth_steps_ = 0;
    std::function<float(DecodingGraphNode::Id, DecodingGraphNode::Id)> growth_policy_ =
        [](DecodingGraphNode::Id start, DecodingGraphNode::Id end)
        {
            return 0.5;
        };


public:
    UnionFindDecoder()
    {
        decoder_name_ = "uf";
    }

    DecodingResult decode(std::shared_ptr<DecodingGraph> graph) override;

    std::vector<DecodingGraphEdge::FusionEdge> grow(const std::shared_ptr<Cluster>& cluster);

    virtual void merge(const std::vector<DecodingGraphEdge::FusionEdge>& fusion_edges);

    int get_last_growth_steps() const override { return last_growth_steps_; }

    void set_growth_policy(const std::function<float(DecodingGraphNode::Id, DecodingGraphNode::Id)> policy) { growth_policy_ = policy; }
};


#endif //CLAYG_UNIONFINDDECODER_H
