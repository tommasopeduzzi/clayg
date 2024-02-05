//
// Created by tommasopeduzzi on 1/28/24.
//

#ifndef CLAYG_CLAYGDECODER_H
#define CLAYG_CLAYGDECODER_H


#include "UnionFindDecoder.h"
#include "Decoder.h"

// ClAYGDecoder inherits from UnionFind
class ClAYGDecoder : private UnionFindDecoder, public virtual Decoder {
    PeelingDecoder m_peeling_decoder;

public:
    ClAYGDecoder();

    std::vector<std::shared_ptr<DecodingGraphEdge>> decode(DecodingGraph graph) override;

    std::vector<std::shared_ptr<DecodingGraphEdge>> clean(DecodingGraph &graph, std::vector<std::shared_ptr<Cluster>> &clusters);

    void add(DecodingGraph& graph, std::vector<std::shared_ptr<Cluster>> &clusters, std::shared_ptr<DecodingGraphNode> node);

    void reset(std::shared_ptr<Cluster> cluster, std::vector<std::shared_ptr<Cluster>> &clusters);
};


#endif //CLAYG_CLAYGDECODER_H
