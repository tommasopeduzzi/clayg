//
// Created by tommasopeduzzi on 1/21/24.
//

#ifndef CLAYG_UNIONFINDDECODER_H
#define CLAYG_UNIONFINDDECODER_H


#include "DecodingGraph.h"
#include "PeelingDecoder.h"
#include "Decoder.h"

class UnionFindDecoder : public virtual Decoder
{
protected:
    std::vector<std::shared_ptr<Cluster>> m_clusters;
    int last_growth_steps_ = 0;

public:
    UnionFindDecoder()
    {
    }

    std::string decoder() override
    {
        return "uf";
    }

    std::vector<std::shared_ptr<DecodingGraphEdge>> decode(std::shared_ptr<DecodingGraph> graph) override;

    std::vector<DecodingGraphEdge::FusionEdge> grow(const std::shared_ptr<Cluster>& cluster);

    void merge(const std::vector<DecodingGraphEdge::FusionEdge>& fusion_edges);

    int get_last_growth_steps() const override { return last_growth_steps_; }
};


#endif //CLAYG_UNIONFINDDECODER_H
