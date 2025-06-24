//
// Created by tommasopeduzzi on 2/5/24.
//

#ifndef CLAYG_DECODER_H
#define CLAYG_DECODER_H

#include <string>

#include "DecodingGraph.h"

class Decoder {
public:
    virtual ~Decoder() = default;

    virtual std::string decoder()
    {
        return "none";
    }

    virtual std::vector<std::shared_ptr<DecodingGraphEdge>> decode(std::shared_ptr<DecodingGraph> graph) {
        return {};
    };

    virtual void dump(const std::string& filename)
    {
    }

    virtual int get_last_growth_steps() const { return 0; }
};


#endif //CLAYG_DECODER_H
