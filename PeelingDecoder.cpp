//
// Created by tommasopeduzzi on 1/21/24.
//

#include <algorithm>
#include <queue>
#include <stdexcept>
#include <iostream>
#include "PeelingDecoder.h"

std::vector<std::shared_ptr<DecodingGraphEdge>>
PeelingDecoder::decode(std::vector<std::shared_ptr<Cluster>> &clusters) {
    std::vector<std::shared_ptr<DecodingGraphEdge>> error_edges = {};
    for (auto cluster: clusters) {
        auto new_error_edges = peel(cluster);
        error_edges.insert(error_edges.end(), new_error_edges.begin(), new_error_edges.end());
    }
    return error_edges;
}

std::pair<std::shared_ptr<DecodingGraphNode>, std::shared_ptr<DecodingGraphEdge>>
PeelingDecoder::find_next_spanning_forest_node(std::shared_ptr<Cluster> &cluster,
                                               std::vector<std::shared_ptr<DecodingGraphNode>> &spanning_forest_nodes) {
#ifdef DEBUG
    std::cout << "Nodes not in cluster: ";
    for (auto node: cluster->nodes()) {
        if (std::find(spanning_forest_nodes.begin(), spanning_forest_nodes.end(), node) ==
            spanning_forest_nodes.end()) {
            std::cout << node->id().id << " ";
        }
    }
    std::cout << std::endl;
#endif
#undef DEBUG

    for (auto node: spanning_forest_nodes) {
        for (auto edge: node->edges()) {
            auto other_node = edge->other_node(node);
            // Check that other_node is in the cluster, but not yet in the spanning forest
            if (other_node->cluster() != cluster) {
                continue;
            }
            // FIXME: Use lookup
            if (std::find(spanning_forest_nodes.begin(), spanning_forest_nodes.end(), other_node) !=
                spanning_forest_nodes.end()) {
                continue;
            }

            spanning_forest_nodes.push_back(other_node);
            return std::make_pair(node, edge);
        }
    }
}

std::vector<std::shared_ptr<DecodingGraphEdge>> PeelingDecoder::peel(std::shared_ptr<Cluster> cluster) {
    std::vector<std::shared_ptr<DecodingGraphNode>> spanning_forest_nodes = {};

    // NOTE: vector<pair<tree_nde, edge>>
    std::vector<std::pair<std::shared_ptr<DecodingGraphNode>, std::shared_ptr<DecodingGraphEdge>>> spanning_forest_edges = {};

    std::shared_ptr<DecodingGraphNode> start_node = nullptr;
    for (auto node: cluster->nodes()) {
        if (node->id().type == DecodingGraphNode::Type::VIRTUAL) {
            start_node = node;
            break;
        }
    }
    if (start_node == nullptr) {
        start_node = cluster->root();
    }
    spanning_forest_nodes.push_back(start_node);

    while (spanning_forest_edges.size() < cluster->nodes().size() - 1) {
        spanning_forest_edges.push_back(find_next_spanning_forest_node(cluster, spanning_forest_nodes));
    }

    std::vector<std::shared_ptr<DecodingGraphEdge>> error_edges = {};

    for (int i = spanning_forest_edges.size() - 1; i >= 0; i--) {
        auto tree_node = spanning_forest_edges[i].first;
        auto edge = spanning_forest_edges[i].second;
        auto leaf_node = edge->other_node(tree_node);

        if (leaf_node->marked()) {
            error_edges.push_back(edge);
            tree_node->set_marked(!tree_node->marked());
            leaf_node->set_marked(false);
        }
    }

    return error_edges;
}
