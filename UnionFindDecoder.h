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

public:
    UnionFindDecoder()
    {
    }

    std::vector<std::shared_ptr<DecodingGraphEdge>> decode(std::shared_ptr<DecodingGraph> graph, bool dump = false, std::string run_id = "") override;

    std::vector<DecodingGraphEdge::FusionEdge> grow(const std::shared_ptr<Cluster>& cluster);

    void merge(const std::vector<DecodingGraphEdge::FusionEdge>& fusion_edges);

    void dump(const std::string& filename) override;
};


#endif //CLAYG_UNIONFINDDECODER_H
