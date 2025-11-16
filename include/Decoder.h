//
// Created by tommasopeduzzi on 2/5/24.
//

#ifndef CLAYG_DECODER_H
#define CLAYG_DECODER_H

#include <string>
#include <unordered_map>
#include <vector>

#include "DecodingGraph.h"

// Type to represent decoding results: pair<vector of error edges, int number of rounds considered>
struct DecodingResult {
    std::vector<std::shared_ptr<DecodingGraphEdge>> corrections;
    int considered_up_to_round;
};

class Decoder {
public:
    explicit Decoder(const std::unordered_map<std::string, std::string>& = {}) { };

    virtual ~Decoder() = default;

    virtual DecodingResult decode(std::shared_ptr<DecodingGraph> graph) {
        return {};
    };

    virtual void dump(const std::string& filename)
    {
    }

    virtual int get_last_growth_steps() const { return 0; }

    std::string decoder_name() {
        return decoder_name_;
    }

    void set_decoder_name(const std::string& decoder_name) {
        decoder_name_ = decoder_name;
    }

protected:
    std::string decoder_name_ = "none";
};


#endif //CLAYG_DECODER_H
