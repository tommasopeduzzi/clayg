//
// Created by tommaso-peduzzi on 10/29/25.
//

#include <iostream>
#include <ostream>
#include <variant>

#include "ClAYGDecoder.h"
#include "DecodingGraph.h"
#include "Logger.h"

using namespace std;

int main()
{
    int D = 10;
    vector<pair<string, unordered_map<string, string>>> decoder_constructors = {
        {"clayg", {}},
        {"clayg", {{"cluster_lifetime", "2"}}},
        {"clayg", {{"stop_early", "true"}}},
        {"clayg", {{"stop_early", "true"}, {"cluster_lifetime", "2"}}},
        {"sl_clayg", {}},
        {"sl_clayg", {{"cluster_lifetime", "2"}}},
        {"sl_clayg", {{"stop_early", "true"}}},
        {"sl_clayg", {{"stop_early", "true"}, {"cluster_lifetime", "2"}}},
        {"uf", {}},
        {"uf", {{"stop_early", "true"}}},
    };
    vector<shared_ptr<Decoder>> decoders;
    for (auto& [decoder_name, args] : decoder_constructors)
    {
        shared_ptr<Decoder> decoder;
        if (decoder_name == "clayg")
            decoder = make_shared<ClAYGDecoder>(args);
        else if (decoder_name == "sl_clayg")
            decoder = make_shared<SingleLayerClAYGDecoder>(args);
        else if (decoder_name == "uf")
            decoder = make_shared<UnionFindDecoder>(args);
        decoders.push_back(decoder);
    }

    shared_ptr<DecodingGraph> decoding_graph = DecodingGraph::repetition_code(D, D);

    logger.set_dump_dir("data/diagrams");
    logger.set_dump_enabled(true);

    variant<string, int> run_id = "d=10";
    auto next_run_id = [](variant<string, int> current_run_id) {
        if (holds_alternative<string>(current_run_id))
            return false;
        current_run_id = get<int>(current_run_id) + 1;
        if (get<int>(current_run_id) > 100)
            return false;
        return true;
    };
    auto run_id_to_string = [](variant<string, int> run_id) {
        if (holds_alternative<string>(run_id))
            return get<string>(run_id);
        return to_string(get<int>(run_id));
    };

    double error_rate = 0.04;
    vector<DecodingGraphEdge::Id> fixed_error_ids = {
        {DecodingGraphEdge::Type::NORMAL, 1,1},
        {DecodingGraphEdge::Type::MEASUREMENT, 2,1},
        {DecodingGraphEdge::Type::MEASUREMENT, 3,2},
        {DecodingGraphEdge::Type::MEASUREMENT, 4,3},
        {DecodingGraphEdge::Type::MEASUREMENT, 5,4},
    };

    do {
        logger.set_run_id(run_id_to_string(run_id));
        logger.prepare_dump_dir();

        decoding_graph->reset();
        logger.log_graph(decoding_graph);

        auto error_ids = fixed_error_ids;
        vector<shared_ptr<DecodingGraphEdge>> error_edges;
        if (fixed_error_ids.empty())
        {
            for (const auto& edge : decoding_graph->edges())
            {
                if ((double) rand() / RAND_MAX < error_rate)
                {
                    error_ids.push_back(edge->id());
                }
            }
        }

        for (const auto& decoder : decoders)
        {
            decoding_graph->reset();
            error_edges = {};
            for (auto error_id : error_ids)
                error_edges.push_back(decoding_graph->edge(error_id).value());
            decoding_graph->mark(error_edges);

            auto [corrections, considered_up_to_round] = decoder->decode(decoding_graph);
            vector<DecodingGraphEdge::Id> correction_ids;
            for (const auto& edge : corrections)
                correction_ids.push_back(edge->id());

            logger.log_errors(error_ids);
            logger.log_corrections(correction_ids, decoder->decoder_name());
        }
    } while (next_run_id(run_id));
}
