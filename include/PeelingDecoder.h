//
// Created by tommasopeduzzi on 1/21/24.
//

#ifndef CLAYG_PEELINGDECODER_H
#define CLAYG_PEELINGDECODER_H


#include "Decoder.h"
#include "DecodingGraph.h"

class PeelingDecoder {
public:
    PeelingDecoder() = default;

    static DecodingResult decode(std::vector<std::shared_ptr<Cluster>>& clusters,
                                                                  const std::shared_ptr<DecodingGraph>& decoding_graph);

    static DecodingResult peel(const std::shared_ptr<Cluster>& cluster,
                                                                const std::shared_ptr<DecodingGraph>& decoding_graph);
};


#endif //CLAYG_PEELINGDECODER_H
