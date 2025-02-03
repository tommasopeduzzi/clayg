//
// Created by tommasopeduzzi on 1/28/24.
//

#include <iostream>
#include "ClAYGDecoder.h"

using namespace std;

vector<shared_ptr<DecodingGraphEdge>> ClAYGDecoder::decode(shared_ptr<DecodingGraph> graph, bool dump, string run_id) {
    vector<shared_ptr<DecodingGraphNode>> marked_nodes;

    for (const auto& node: graph->nodes()) {
        if (node->marked()) {
            marked_nodes.push_back(node);
        }
    }

    struct decoding_graph_node_comparator
    {
        inline bool operator() (const shared_ptr<DecodingGraphNode>& node1, const shared_ptr<DecodingGraphNode>& node2)
        {
            if (node1->id().round == node2->id().round) {
                return node1->id().id < node2->id().id;
            }
            return node1->id().round < node2->id().round;
        }
    };

    sort(marked_nodes.begin(), marked_nodes.end(), decoding_graph_node_comparator());

    const int rounds = graph->t();
    auto flattened_graph = DecodingGraph::rotated_surface_code(graph->d(), 1);

    if (dump) {
        flattened_graph->dump("runs/" + run_id + "/graph.txt");
    }

    vector<shared_ptr<DecodingGraphEdge>> error_edges;

    m_clusters = {};
    int g = 1;
    for (int round = 0; round <= rounds; round++) {
        vector<shared_ptr<DecodingGraphNode>> nodes;
        for (const auto& node: marked_nodes) {
            if (node->id().round == round && node->marked()) {
                nodes.push_back(node);
            }
        }
        for (const auto& node: nodes) {
            add(flattened_graph, node);
        }
        auto new_error_edges = clean(flattened_graph);
        if (dump)
            this->dump("runs/" + run_id + "/clusters_" + to_string(round*2) + ".txt");
        error_edges.insert(error_edges.end(), new_error_edges.begin(), new_error_edges.end());
        for (int _ = 0; _ < g; _++) {
            vector<DecodingGraphEdge::FusionEdge> fusion_edges;
            for (const auto& cluster: m_clusters) {
                if (cluster->is_neutral()) continue;
                auto new_fusion_edges = grow(cluster);
                for (const auto& fusion_edge : new_fusion_edges) {
                    fusion_edges.push_back(fusion_edge);
                }
            }
            if (dump) {
                this->dump("runs/" + run_id + "/clusters_" + to_string(round*2 + 1) + ".txt");
            }
            merge(fusion_edges);
        }
        new_error_edges = clean(flattened_graph);
        error_edges.insert(error_edges.end(), new_error_edges.begin(), new_error_edges.end());
    }

    while (!Cluster::all_clusters_are_neutral(m_clusters)) {
        vector<DecodingGraphEdge::FusionEdge> fusion_edges;
        for (const auto& cluster: m_clusters) {
            if (cluster->is_neutral()) continue;
            auto new_fusion_edges = grow(cluster);
            for (const auto& fusion_edge : new_fusion_edges) {
                fusion_edges.push_back(fusion_edge);
            }
        }
        merge(fusion_edges);
    }

    auto new_error_edges = clean(flattened_graph);
    error_edges.insert(error_edges.end(), new_error_edges.begin(), new_error_edges.end());

    return error_edges;
}

void ClAYGDecoder::add(const shared_ptr<DecodingGraph>& graph, shared_ptr<DecodingGraphNode> node) {
    // Find corresponding node in the flattened decoding graph
    auto id = node->id();
    id.round = 0;
    node = graph->node(id).value();
    node->set_marked(!node->marked());
    if (node->cluster().has_value())
    {
        auto cluster = node->cluster().value().lock();
        if (node->marked())
        {
            cluster->add_marked_node(node);
        }
        else
        {
            cluster->remove_marked_node(node);
        }
    }
    else
    {
        auto new_cluster = make_shared<Cluster>(node);
        node->set_cluster(new_cluster);
        if (node->marked())
        {
            new_cluster->add_marked_node(node);
        }
        if (node->id().type == DecodingGraphNode::VIRTUAL)
        {
            new_cluster->add_virtual_node(node);
        }
        m_clusters.push_back(new_cluster);
    }
}

vector<shared_ptr<DecodingGraphEdge>> ClAYGDecoder::clean(const shared_ptr<DecodingGraph>& decoding_graph) {
    vector<shared_ptr<DecodingGraphEdge>> error_edges;
    vector<shared_ptr<Cluster>> new_clusters;
    for (auto& cluster: m_clusters) {
        if (!cluster->is_neutral())
        {
            new_clusters.push_back(move(cluster));
            continue;
        }

        auto new_error_edges = PeelingDecoder::peel(cluster, decoding_graph);
        error_edges.insert(error_edges.end(), new_error_edges.begin(), new_error_edges.end());

        for (const auto& node: cluster->nodes()) {
            node->set_cluster(nullopt);
        }
        for (const auto& edge: cluster->edges())
        {
            edge->reset_growth();
        }

        for (const auto& boundary_edge: cluster->boundary())
        {
            boundary_edge.edge->reset_growth();
        }
    }
    m_clusters = move(new_clusters);
    return error_edges;
}
