//
// Created by tommasopeduzzi on 1/21/24.
//

#include <algorithm>
#include <queue>
#include <stdexcept>
#include <iostream>
#include "PeelingDecoder.h"

using namespace std;

DecodingResult PeelingDecoder::decode(vector<shared_ptr<Cluster>>& clusters,
                                                             const shared_ptr<DecodingGraph>&
                                                             decoding_graph)
{
    vector<shared_ptr<DecodingGraphEdge>> error_edges = {};
    for (auto& cluster : clusters)
    {
        if (cluster->marked_nodes().empty())
        {
            continue;
        }
        auto new_error_edges = peel(cluster, decoding_graph);
        error_edges.insert(error_edges.end(), new_error_edges.begin(), new_error_edges.end());
    }
    return { error_edges, decoding_graph->t() };
}

vector<shared_ptr<DecodingGraphEdge>> PeelingDecoder::peel(shared_ptr<Cluster>& cluster,
                                                           const shared_ptr<DecodingGraph>&
                                                           decoding_graph)
{
    vector<shared_ptr<DecodingGraphNode>> spanning_forest_nodes = {};

    // NOTE: vector<pair<tree_nde, edge>>
    vector<pair<shared_ptr<DecodingGraphNode>, shared_ptr<DecodingGraphEdge>>> spanning_forest_edges
        = {};

    shared_ptr<DecodingGraphNode> start_node = nullptr;
    for (const auto& node : cluster->nodes())
    {
        ;
        if (node->id().type == DecodingGraphNode::Type::VIRTUAL)
        {
            start_node = node;
            break;
        }
    }
    if (start_node == nullptr)
    {
        start_node = cluster->root().lock();
    }
    spanning_forest_nodes.push_back(start_node);

    while (spanning_forest_edges.size() < cluster->nodes().size() - 1)
    {
        bool edge_added = false; // Flag to check if an edge was added in this iteration

        for (const auto& current_node : spanning_forest_nodes)
        {
            shared_ptr<DecodingGraphEdge> edge;
            shared_ptr<DecodingGraphNode> other_node;

            for (const auto& edge_weak_ptr : current_node->edges())
            {
                edge = edge_weak_ptr.lock();
                other_node = edge->other_node(current_node).lock();

                // check that node is in the same cluster
                if (!other_node->cluster().has_value())
                {
                    continue;
                }
                if (other_node->cluster().value().lock() != cluster)
                {
                    continue;
                }

                bool already_in_forest = false;
                for (const auto& spanning_forest_node : spanning_forest_nodes)
                {
                    if (spanning_forest_node->id() == other_node->id())
                    {
                        already_in_forest = true;
                        break;
                    }
                }

                if (!already_in_forest)
                {
                    spanning_forest_edges.emplace_back(current_node, edge);
                    spanning_forest_nodes.push_back(other_node);
                    edge_added = true;
                    break;
                }
            }

            if (edge_added)
            {
                break;
            }
        }

        if (!edge_added)
        {
            throw runtime_error("No valid edge found to add to the spanning forest");
        }
    }

    vector<shared_ptr<DecodingGraphEdge>> error_edges = {};

    for (int i = static_cast<int>(spanning_forest_edges.size() - 1); i >= 0; i--)
    {
        auto tree_node = spanning_forest_edges[i].first;
        const auto edge = spanning_forest_edges[i].second;
        auto leaf_node = edge->other_node(tree_node).lock();

        if (leaf_node->marked())
        {
            error_edges.push_back(edge);
            tree_node->set_marked(!tree_node->marked());
            leaf_node->set_marked(false);
        }
    }

    return error_edges;
}
