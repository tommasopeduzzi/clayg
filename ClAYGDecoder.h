//
// Created by tommasopeduzzi on 1/28/24.
//

#ifndef CLAYG_CLAYGDECODER_H
#define CLAYG_CLAYGDECODER_H


#include "UnionFindDecoder.h"
#include "Decoder.h"

// ClAYGDecoder inherits from UnionFind
// It owns the clusters and the graph, but not its constituents
class ClAYGDecoder final : UnionFindDecoder, public virtual Decoder {
public:
    ClAYGDecoder() = default;

    std::vector<std::shared_ptr<DecodingGraphEdge>> decode(std::shared_ptr<DecodingGraph> graph, bool dump = false, std::string run_id = "") override;

    std::vector<std::shared_ptr<DecodingGraphEdge>> clean(const std::shared_ptr<DecodingGraph>& decoding_graph);

    void add(const std::shared_ptr<DecodingGraph>& graph, std::shared_ptr<DecodingGraphNode> node);
};


#endif //CLAYG_CLAYGDECODER_H
