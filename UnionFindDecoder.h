//
// Created by tommasopeduzzi on 1/21/24.
//

#ifndef CLAYG_UNIONFINDDECODER_H
#define CLAYG_UNIONFINDDECODER_H


#include "DecodingGraph.h"
#include "PeelingDecoder.h"
#include "Decoder.h"

class UnionFindDecoder : public virtual Decoder {
    PeelingDecoder m_peeling_decoder;

public:
    UnionFindDecoder() {}

    std::vector<std::shared_ptr<DecodingGraphEdge>> decode(std::shared_ptr<DecodingGraph> graph) override;

    std::set<std::shared_ptr<DecodingGraphEdge>> grow(std::shared_ptr<Cluster> cluster);

    void merge(std::set<std::shared_ptr<DecodingGraphEdge>> fusion_edges, std::vector<std::shared_ptr<Cluster>> &clusters);
};


#endif //CLAYG_UNIONFINDDECODER_H
