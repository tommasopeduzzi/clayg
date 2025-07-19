//
// Created by tommasopeduzzi on 1/28/24.
//

#ifndef CLAYG_CLAYGDECODER_H
#define CLAYG_CLAYGDECODER_H


#include "UnionFindDecoder.h"
#include "Decoder.h"

// ClAYGDecoder inherits from UnionFind
// It owns the clusters and the graph, but not its constituents
class ClAYGDecoder : public virtual UnionFindDecoder {
public:
    ClAYGDecoder()
    {
        decoder_name_ = "clayg";
    }

    std::vector<std::shared_ptr<DecodingGraphEdge>> decode(std::shared_ptr<DecodingGraph> graph) override;

    std::vector<std::shared_ptr<DecodingGraphEdge>> clean(const std::shared_ptr<DecodingGraph>& decoding_graph);

    virtual void add(const std::shared_ptr<DecodingGraph>& graph, std::shared_ptr<DecodingGraphNode> node);
};

class SingleLayerClAYGDecoder : public virtual ClAYGDecoder
{
public:
    SingleLayerClAYGDecoder()
    {
        decoder_name_ = "sl_clayg";
    }

    std::vector<std::shared_ptr<DecodingGraphEdge>> decode(std::shared_ptr<DecodingGraph> graph) override;

    void add(const std::shared_ptr<DecodingGraph>& graph, std::shared_ptr<DecodingGraphNode> node) override;
};

#endif //CLAYG_CLAYGDECODER_H
