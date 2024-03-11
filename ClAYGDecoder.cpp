//
// Created by tommasopeduzzi on 1/28/24.
//

#include <iostream>
#include "ClAYGDecoder.h"

ClAYGDecoder::ClAYGDecoder() {
    m_peeling_decoder = PeelingDecoder();
}

std::vector<std::shared_ptr<DecodingGraphEdge>> ClAYGDecoder::decode(DecodingGraph graph) {
    std::vector<std::shared_ptr<DecodingGraphNode>> marked_nodes;


    for (auto node: graph.nodes()) {
        if (node->marked()) {
            marked_nodes.push_back(node);
        }
    }

    struct decoding_graph_node_comparator
    {
        inline bool operator() (const std::shared_ptr<DecodingGraphNode>& node1, const std::shared_ptr<DecodingGraphNode>& node2)
        {
            if (node1->id().round == node2->id().round) {
                return node1->id().id < node2->id().id;
            }
            return node1->id().round < node2->id().round;
        }
    };

    std::sort(marked_nodes.begin(), marked_nodes.end(), decoding_graph_node_comparator());

    int rounds = graph.t();
    graph = DecodingGraph::rotated_surface_code(graph.d(), 1);

    std::vector<std::shared_ptr<Cluster>> clusters;

    // FIXME: Do we actually need to keep track of all clusters?
    for (auto node : graph.nodes()) {
        clusters.push_back(node->cluster());
    }

    std::vector<std::shared_ptr<DecodingGraphEdge>> error_edges;

    int g = 1;
    for (int round = 0; round <= rounds; round++) {
        std::vector<std::shared_ptr<DecodingGraphNode>> nodes;
        for (auto node: marked_nodes) {
            if (node->id().round == round && node->marked() && node->id().type == DecodingGraphNode::Type::ANCILLA) {
                nodes.push_back(node);
            }
        }
        for (auto node: nodes) {
            add(graph, clusters, node);
        }
        // clean(graph, clusters);
        for (int _ = 0; _ < g; _++) {
            std::set<std::shared_ptr<DecodingGraphEdge>> fusion_edges;
            for (auto cluster: clusters) {
                if (cluster->is_neutral()) continue;
                auto new_fusion_edges = grow(cluster);
                fusion_edges.insert(new_fusion_edges.begin(), new_fusion_edges.end());
            }
            merge(fusion_edges, clusters);
        }
        auto new_error_edges = clean(graph, clusters);
        error_edges.insert(error_edges.end(), new_error_edges.begin(), new_error_edges.end());
    }

    return error_edges;
}

void ClAYGDecoder::add(DecodingGraph &graph, std::vector<std::shared_ptr<Cluster>> &clusters, std::shared_ptr<DecodingGraphNode> node) {
    // Find corresponding node in the flattened decoding graph
    auto id = node->id();
    id.round = 0;
    node = graph.node(id).value();
    node->set_marked(!node->marked());
    if (node->marked())
        node->cluster()->addMarkedNode(node);
    else
        node->cluster()->removeMarkedNode(node);
}

std::vector<std::shared_ptr<DecodingGraphEdge>> ClAYGDecoder::clean(DecodingGraph &graph, std::vector<std::shared_ptr<Cluster>> &clusters) {
    std::vector<std::shared_ptr<DecodingGraphEdge>> error_edges;
    std::vector<std::shared_ptr<Cluster>> clusters_to_remove;
    for (auto cluster: clusters) {
        if (!cluster->is_neutral() || cluster->marked_nodes().size() == 0) continue;


        auto new_error_edges = m_peeling_decoder.peel(cluster);
        error_edges.insert(error_edges.end(), new_error_edges.begin(), new_error_edges.end());

        // remove cluster from clusters
        reset(cluster, clusters);
        clusters_to_remove.push_back(cluster);
    }
    for (auto cluster : clusters_to_remove) {
        clusters.erase(std::find(clusters.begin(), clusters.end(), cluster));
    }
    return error_edges;
}

void ClAYGDecoder::reset(std::shared_ptr<Cluster> cluster, std::vector<std::shared_ptr<Cluster>> &clusters) {

    for (auto node: cluster->nodes()) {
        node->set_marked(false);
        node->set_cluster(std::make_shared<Cluster>(node));
        clusters.push_back(node->cluster());
    }

    for (auto edge: cluster->edges() ) {
        edge->add_growth(-edge->growth());
    }

    for (auto edge: cluster->boundary()) {
        edge->add_growth(-edge->growth());
    }

}