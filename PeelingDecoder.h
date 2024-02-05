//
// Created by tommasopeduzzi on 1/21/24.
//

#ifndef CLAYG_PEELINGDECODER_H
#define CLAYG_PEELINGDECODER_H


#include "DecodingGraph.h"

class PeelingDecoder {
    DecodingGraph m_graph;

public:
    PeelingDecoder() {}

    std::vector<std::shared_ptr<DecodingGraphEdge>> decode(std::vector<std::shared_ptr<Cluster>> &clusters);

    std::vector<std::shared_ptr<DecodingGraphEdge>> peel(std::shared_ptr<Cluster> cluster);

    std::pair<std::shared_ptr<DecodingGraphNode>, std::shared_ptr<DecodingGraphEdge>> find_next_spanning_forest_node(std::shared_ptr<Cluster> &cluster,
                                                                                   std::vector<std::shared_ptr<DecodingGraphNode>> &spanning_forest_nodes);
};


#endif //CLAYG_PEELINGDECODER_H
