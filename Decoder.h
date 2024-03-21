//
// Created by tommasopeduzzi on 2/5/24.
//

#ifndef CLAYG_DECODER_H
#define CLAYG_DECODER_H

#include "DecodingGraph.h"

class Decoder {
public:
    virtual std::vector<std::shared_ptr<DecodingGraphEdge>> decode(std::shared_ptr<DecodingGraph> graph) {
        return {};
    };

};


#endif //CLAYG_DECODER_H
