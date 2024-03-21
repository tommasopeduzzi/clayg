#include <iostream>
#include <algorithm>
#include <random>
#include "DecodingGraph.h"
#include "UnionFindDecoder.h"
#include "ClAYGDecoder.h"

// #define DEBUG

int main(int argc, char *argv[]) {
    // Parse command line arguments in format D T p_start p_end p_step decoder
    if (argc != 7) {
        std::cerr << "Usage: " << argv[0] << " D T p_start p_end p_step decoder" << std::endl;
        return 1;
    }

    int D = std::stoi(argv[1]);
    int T = std::stoi(argv[2]);
    double p = std::stod(argv[3]);
    double p_end = std::stod(argv[4]);
    double p_step = std::stod(argv[5]);
    std::string decoder_type = argv[6];

    std::unique_ptr<Decoder> decoder = {};
    if (decoder_type == "none") {
        decoder = std::make_unique<Decoder>();
    } else if (decoder_type == "unionfind") {
        decoder = std::make_unique<UnionFindDecoder>();
    } else if (decoder_type == "clayg") {
        decoder = std::make_unique<ClAYGDecoder>();
    } else {
        std::cerr << "Invalid decoder type: " << decoder_type << std::endl;
        return 1;
    }

    auto graph = DecodingGraph::rotated_surface_code(D, T);
    std::vector<std::shared_ptr<DecodingGraphEdge>> error_edges {};

    std::random_device rd;
    std::mt19937 gen(rd());

    while (p <= p_end) {
        int logicals = 0;
        for (int i = 0; i < 10000; i++) {
            graph->reset();

            error_edges.clear();

            // Random float between 0 and 1
            std::uniform_real_distribution<> dis(0, 1);
            for (int t = 0; t < T; t++) {
                for (int edge = 0; edge < D * D; edge++) {
                    if (dis(gen) <= p) {
                        auto id = DecodingGraphEdge::Id{DecodingGraphEdge::Type::NORMAL, t, edge};
                        error_edges.push_back(graph->edge(id).value());
                    }
                }
                if (t == T - 1) continue;
                for (int edge = 0; edge < (D - 1) * (int(D / 2) + 1); edge++) {
                    if (dis(gen) <= p) {
                        auto id = DecodingGraphEdge::Id{DecodingGraphEdge::Type::MEASUREMENT, t, edge};
                        error_edges.push_back(graph->edge(id).value());
                    }
                }
            }
#ifdef DEBUG
            std::cout << "Error edges: " << error_edges.size() << std::endl;
            for (auto edge: error_edges) {
                std::cout << edge->id().round << ": " << edge->id().id << ", Type : "
                          << (edge->id().type == DecodingGraphEdge::Type::MEASUREMENT ? "Measurement" : "Normal")
                          << std::endl;
            }
#endif

            auto initial_error_edges = error_edges;

            for (auto edge: error_edges) {
                auto nodes = {edge->nodes().first, edge->nodes().second};
                for (auto node: nodes) {
                    if (node->id().type == DecodingGraphNode::VIRTUAL) {
                        continue;
                    }
                    if (node->marked()) {
                        node->set_marked(false);
                        node->cluster()->removeMarkedNode(node);
                    } else {
                        node->set_marked(true);
                        node->cluster()->addMarkedNode(node);
                    }
                }
            }

            auto final_error_edges = decoder->decode(graph);
            error_edges.insert(error_edges.end(), final_error_edges.begin(), final_error_edges.end());

#ifdef DEBUG
            // output final error edges
            std::cout << "Final error edges: " << final_error_edges.size() << std::endl;
            for (auto edge: final_error_edges) {
                std::cout << edge->id().round << ": " << edge->id().id << ", Type : "
                          << (edge->id().type == DecodingGraphEdge::Type::MEASUREMENT ? "Measurement" : "Normal")
                          << std::endl;
            }
#endif
            // FIXME: Update checking to work?!
            auto logical_edge_ids = graph->logical_edge_ids();
            int logical = 0;
            for (auto error: error_edges) {
                // Check if edge is relevant to final measurement
                if (error->id().type == DecodingGraphEdge::Type::MEASUREMENT)
                    continue;

                if (std::find(logical_edge_ids.begin(), logical_edge_ids.end(), error->id().id) == logical_edge_ids.end())
                    continue;

                logical += 1;
            }

            logical = logical % 2;

            logicals += logical;


#ifdef DEBUG
            if (logical == 1) {
                std::cout << "Error edges: " << initial_error_edges.size() << std::endl;
                for (auto edge: initial_error_edges) {
                    std::cout << edge->id().round << ": " << edge->id().id << ", Type : "
                              << (edge->id().type == DecodingGraphEdge::Type::MEASUREMENT ? "Measurement" : "Normal")
                              << std::endl;
                }
                std::cout << "Final error edges: " << final_error_edges.size() << std::endl;
                for (auto edge: final_error_edges) {
                    std::cout << edge->id().round << ": " << edge->id().id << ", Type : "
                              << (edge->id().type == DecodingGraphEdge::Type::MEASUREMENT ? "Measurement" : "Normal")
                              << std::endl;
                }
                std::cout << "Logical: " << logical << std::endl;
            }
#endif

        }

        double error_rate = double(logicals) / 10000;
        std::cout << p << "\t" << error_rate << std::endl;
        p += p_step;
    }

    return 0;
}
