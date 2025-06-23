//
// Created by tommasopeduzzi on 2/5/24.
//

#ifndef CLAYG_DECODER_H
#define CLAYG_DECODER_H

#include <string>

#include "DecodingGraph.h"

class Decoder {
public:
    // FIXME: DO LOGGING DIFFERENTLY!
    int last_growth_steps = 0;

    virtual ~Decoder() = default;

    virtual std::string decoder()
    {
        return "none";
    }

    virtual std::vector<std::shared_ptr<DecodingGraphEdge>> decode(std::shared_ptr<DecodingGraph> graph, bool dump = false, std::string run_id = "") {
        return {};
    };

    virtual void dump(const std::string& filename)
    {
    }
};


#endif //CLAYG_DECODER_H
