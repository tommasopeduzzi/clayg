//
// Created by tommasopeduzzi on 1/21/24.
//

#ifndef CLAYG_PEELINGDECODER_H
#define CLAYG_PEELINGDECODER_H


#include "DecodingGraph.h"

class PeelingDecoder {
public:
    PeelingDecoder() = default;

    static std::vector<std::shared_ptr<DecodingGraphEdge>> decode(std::vector<std::shared_ptr<Cluster>>& clusters,
                                                                  const std::shared_ptr<DecodingGraph>& decoding_graph);

    static std::vector<std::shared_ptr<DecodingGraphEdge>> peel(std::shared_ptr<Cluster>& cluster,
                                                                const std::shared_ptr<DecodingGraph>& decoding_graph);

    static std::pair<std::shared_ptr<DecodingGraphNode>, std::shared_ptr<DecodingGraphEdge>>
    find_next_spanning_forest_node(std::shared_ptr<Cluster>& cluster,
                                   std::vector<std::shared_ptr<DecodingGraphNode>>& spanning_forest_nodes, const std::shared_ptr<
                                       DecodingGraph>& decoding_graph);
};


#endif //CLAYG_PEELINGDECODER_H
