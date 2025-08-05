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
protected:
    int growth_rounds_ = 1;
    bool stop_early_ = false;
    std::shared_ptr<DecodingGraph> decoding_graph_;
public:
    explicit ClAYGDecoder(const int growth_rounds = 1, const bool stop_early = false)
        : growth_rounds_(growth_rounds), stop_early_(stop_early)
    {
        decoder_name_ = "clayg";
    }

    DecodingResult decode(std::shared_ptr<DecodingGraph> graph) override;

    std::vector<std::shared_ptr<DecodingGraphEdge>> clean(const std::shared_ptr<DecodingGraph>& decoding_graph);

    virtual void add(const std::shared_ptr<DecodingGraph>& graph, std::shared_ptr<DecodingGraphNode> node);

    void set_growth_rounds(const int growth_rounds) { growth_rounds_ = growth_rounds; }

    void set_stop_early(const bool stop_early) { stop_early_ = stop_early; }
};

class SingleLayerClAYGDecoder : public virtual ClAYGDecoder
{
private:
public:
    explicit SingleLayerClAYGDecoder(const int growth_rounds = 1, const bool stop_early = false)
        : ClAYGDecoder(growth_rounds, stop_early)
    {
        decoder_name_ = "sl_clayg";
        decoding_graph_ = DecodingGraph::rotated_surface_code(2, 2);
    }

    DecodingResult decode(std::shared_ptr<DecodingGraph> graph) override;

    void add(const std::shared_ptr<DecodingGraph>& graph, std::shared_ptr<DecodingGraphNode> node) override;
};

#endif //CLAYG_CLAYGDECODER_H
