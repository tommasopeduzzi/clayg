//
// Created by tommaso-peduzzi on 10/29/25.
//

#include <iostream>
#include <ostream>

#include "ClAYGDecoder.h"
#include "DecodingGraph.h"
#include "Logger.h"

using namespace std;

int main()
{
    int D = 6;

    shared_ptr<DecodingGraph> decoding_graph = DecodingGraph::repetition_code(D, D);

    logger.set_dump_dir("data/diagrams");
    logger.set_dump_enabled(true);

    for (int run_id = 0; run_id < 1; run_id++)
    {
        logger.set_run_id(run_id);
        logger.prepare_dump_dir();

        decoding_graph->reset();
        logger.log_graph(decoding_graph);

        vector<DecodingGraphEdge::Id> error_ids;
        vector<shared_ptr<DecodingGraphEdge>> error_edges;
        for (auto edge : decoding_graph->edges())
        {
            if (((double) rand() / RAND_MAX) < 0.04)
            {
                error_ids.push_back(edge->id());
                error_edges.push_back(edge);
            }
        }
        error_ids = {
            {DecodingGraphEdge::Type::MEASUREMENT, 3,3},
            {DecodingGraphEdge::Type::NORMAL, 3,3},
            {DecodingGraphEdge::Type::NORMAL, 1,1},
            {DecodingGraphEdge::Type::MEASUREMENT, 5,1},
        };
        error_edges = {};
        for (auto error_id : error_ids)
            error_edges.push_back(decoding_graph->edge(error_id).value());

        decoding_graph->mark(error_edges);

        auto decoder_args = unordered_map<string, string>{
            {"stop_early", "true"}
        };
        const auto decoder = make_shared<ClAYGDecoder>(decoder_args);
        auto [corrections, considered_up_to_round] = decoder->decode(decoding_graph);
        vector<DecodingGraphEdge::Id> correction_ids;
        for (const auto& edge : corrections)
            correction_ids.push_back(edge->id());

        logger.log_errors(error_ids);
        logger.log_corrections(correction_ids, decoder->decoder_name());
    }
}
