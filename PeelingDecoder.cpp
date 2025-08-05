//
// Created by tommasopeduzzi on 1/21/24.
//

#include <algorithm>
#include <queue>
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

    map<DecodingGraphNode::Id, bool> visited_nodes;
    for (const auto& node : cluster->nodes())
    {
        visited_nodes[node->id()] = false;
    }

    std::queue<std::shared_ptr<DecodingGraphNode>> node_queue;
    node_queue.push(start_node);
    visited_nodes[start_node->id()] = true;

    while (spanning_forest_edges.size() < cluster->nodes().size() - 1 && !node_queue.empty())
    {
        auto current_node = node_queue.front();
        node_queue.pop();

        for (const auto& edge_weak : current_node->edges())
        {
            auto edge = edge_weak.lock();

            auto neighbor = edge->other_node(current_node).lock();
            if (!neighbor || visited_nodes[neighbor->id()]) continue;
            if (!neighbor->cluster().has_value())
            {
                continue; // skip nodes not in a cluster
            }
            if (neighbor->cluster().value().lock() != cluster)
            {
                continue;
            }

            visited_nodes[neighbor->id()] = true;
            spanning_forest_edges.emplace_back(current_node, edge);
            node_queue.push(neighbor);
        }
    }


    vector<shared_ptr<DecodingGraphEdge>> error_edges = {};

    // collect list of node ids in the spanning forest
    vector<DecodingGraphNode::Id> spanning_forest_node_ids;
    for (const auto& edge_pair : spanning_forest_edges)
    {
        auto tree_node = edge_pair.first;
        if (std::find(spanning_forest_node_ids.begin(), spanning_forest_node_ids.end(), tree_node->id())
            == spanning_forest_node_ids.end())
        {
            spanning_forest_node_ids.push_back(tree_node->id());
        }
    }
    vector<DecodingGraphEdge::Id> spanning_forest_edge_ids;
    for (const auto& edge_pair : spanning_forest_edges)
    {
        auto edge = edge_pair.second;
        spanning_forest_edge_ids.push_back(edge->id());
    }

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
