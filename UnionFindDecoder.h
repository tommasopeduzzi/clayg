//
// Created by tommasopeduzzi on 1/21/24.
//

#ifndef CLAYG_UNIONFINDDECODER_H
#define CLAYG_UNIONFINDDECODER_H


#include "DecodingGraph.h"
#include "PeelingDecoder.h"
#include "Decoder.h"

class UnionFindDecoder : public virtual Decoder {
public:
    UnionFindDecoder() {}

    std::vector<std::shared_ptr<DecodingGraphEdge>> decode(std::shared_ptr<DecodingGraph> graph) override;

    static std::vector<DecodingGraphEdge::FusionEdge> grow(const std::shared_ptr<Cluster>& cluster);

    static void merge(const std::vector<DecodingGraphEdge::FusionEdge>& fusion_edges, std::vector<std::shared_ptr<Cluster>>& clusters);
};


#endif //CLAYG_UNIONFINDDECODER_H
