//
// Created by tommasopeduzzi on 1/28/24.
//

#ifndef CLAYG_CLAYGDECODER_H
#define CLAYG_CLAYGDECODER_H


#include "UnionFindDecoder.h"
#include "Decoder.h"

// ClAYGDecoder inherits from UnionFind
// It owns the clusters and the graph, but not its constituents
class ClAYGDecoder final : private UnionFindDecoder, public virtual Decoder {
public:
    ClAYGDecoder() = default;

    std::vector<std::shared_ptr<DecodingGraphEdge>> decode(std::shared_ptr<DecodingGraph> graph) override;

    static std::vector<std::shared_ptr<DecodingGraphEdge>> clean(std::vector<std::shared_ptr<Cluster>>& clusters,
                                                                 const std::shared_ptr<DecodingGraph>& decoding_graph);

    static void add(const std::shared_ptr<DecodingGraph>& graph, std::vector<std::shared_ptr<Cluster>> &clusters, std::shared_ptr<DecodingGraphNode> node);

    static void reset(const std::shared_ptr<Cluster>& cluster);
};


#endif //CLAYG_CLAYGDECODER_H
