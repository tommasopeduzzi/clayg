#include <iostream>
#include <algorithm>
#include <random>
#include "DecodingGraph.h"
#include "UnionFindDecoder.h"
#include "ClAYGDecoder.h"

using namespace std;

int main(int argc, char* argv[])
{
    // Parse command line arguments in format D T p_start p_end p_step decoder
    if (argc != 7)
    {
        cerr << "Usage: " << argv[0] << " D T p_start p_end p_step decoder" << endl;
        return 1;
    }

    int D = stoi(argv[1]);
    int T = stoi(argv[2]);
    double p = stod(argv[3]);
    double p_end = stod(argv[4]);
    double p_step = stod(argv[5]);
    string decoder_type = argv[6];

    unique_ptr<Decoder> decoder = {};
    if (decoder_type == "none")
    {
        decoder = make_unique<Decoder>();
    }
    else if (decoder_type == "unionfind")
    {
        decoder = make_unique<UnionFindDecoder>();
    }
    else if (decoder_type == "clayg")
    {
        decoder = make_unique<ClAYGDecoder>();
    }
    else
    {
        throw invalid_argument("Invalid decoder type: " + decoder_type);
    }

    auto graph = DecodingGraph::rotated_surface_code(D, T);
    vector<shared_ptr<DecodingGraphEdge>> error_edges{};
    vector<DecodingGraphEdge::Id> error_edge_ids{};

    random_device rd;
    mt19937 gen(rd());

    auto uf_decoder = make_unique<UnionFindDecoder>();
    auto clayg_decoder = make_unique<ClAYGDecoder>();

    while (p <= p_end)
    {
        int logicals = 0;
        uniform_real_distribution<> dis(0, 1);

        for (int i = 0; i < 10000; i++)
        {
            error_edge_ids.clear();
            for (int t = 0; t < T; t++)
            {
                for (int edge = 0; edge < D * D; edge++)
                {
                    if (dis(gen) <= p)
                    {
                        auto id = DecodingGraphEdge::Id{DecodingGraphEdge::Type::NORMAL, t, edge};
                        error_edge_ids.push_back(id);
                    }
                }
                if (t == T - 1) continue;
                for (int edge = 0; edge < (D - 1) * (int(D / 2) + 1); edge++)
                {
                    if (dis(gen) <= p)
                    {
                        auto id = DecodingGraphEdge::Id{DecodingGraphEdge::Type::MEASUREMENT, t, edge};
                        error_edge_ids.push_back(id);
                    }
                }
            }


            graph->reset();
            error_edges.clear();
            for (auto id : error_edge_ids)
            {
                auto edge = graph->edge(id).value();
                error_edges.push_back(edge);
            }

            auto logical_edge_ids = graph->logical_edge_ids();
            int logical = 0;

            // Compute logical before correction
            for (const auto& error : error_edges)
            {
                // Check if edge is relevant to final measurement
                if (error->id().type == DecodingGraphEdge::Type::MEASUREMENT)
                    continue;

                if (logical_edge_ids.find(error->id().id) == logical_edge_ids.end())
                    continue;

                logical += 1;
            }

            for (const auto& edge : error_edges)
            {
                auto nodes = {edge->nodes().first, edge->nodes().second};
                for (const auto& node_weak_ptr : nodes)
                {
                    auto node = node_weak_ptr.lock();
                    if (node->id().type == DecodingGraphNode::VIRTUAL)
                    {
                        continue;
                    }
                    if (node->marked())
                    {
                        node->set_marked(false);
                    }
                    else
                    {
                        node->set_marked(true);
                    }
                }
            }

            auto corrections = decoder->decode(graph);

            for (const auto& error : corrections)
            {
                // Check if edge is relevant to final measurement
                if (error->id().type == DecodingGraphEdge::Type::MEASUREMENT)
                    continue;

                if (logical_edge_ids.find(error->id().id) == logical_edge_ids.end())
                    continue;

                logical += 1;
            }
            logical %= 2;
            logicals += logical;
        }

        double error_rate = static_cast<double>(logicals) / 10000;
        cout << p << "\t" << error_rate << endl;
        p += p_step;
    }

    return 0;
}
