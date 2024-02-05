//
// Created by tommasopeduzzi on 1/21/24.
//

#include <algorithm>
#include <iostream>
#include "UnionFindDecoder.h"

std::vector<std::shared_ptr<DecodingGraphEdge>> UnionFindDecoder::decode(DecodingGraph graph) {
    std::vector<std::shared_ptr<Cluster>> clusters;
    for (auto node: graph.nodes()) {
        if (!node->cluster()->is_neutral())
            clusters.push_back(node->cluster());
    }
    int i = 0;

    while (!Cluster::all_clusters_are_neutral(clusters)) {
        std::set<std::shared_ptr<DecodingGraphEdge>> fusion_edges;
        for (auto cluster: clusters) {
            if (cluster->is_neutral()) continue;
            auto new_fusion_edges = grow(cluster);
            fusion_edges.insert(new_fusion_edges.begin(), new_fusion_edges.end());
        }
        merge(fusion_edges, clusters);
        if (i > 1000) {
            std::cout << "Too many iterations" << std::endl;
        }
        i++;
    }

    return m_peeling_decoder.decode(clusters);
}

std::set<std::shared_ptr<DecodingGraphEdge>> UnionFindDecoder::grow(std::shared_ptr<Cluster> cluster) {
    if (cluster->is_neutral()) return {};
    std::set<std::shared_ptr<DecodingGraphEdge>> fusion_edges;
    std::vector<std::shared_ptr<DecodingGraphEdge>> boundary_edges;
    for (auto boundary_edge: cluster->boundary()) {
        if (boundary_edge->nodes().first->cluster() == boundary_edge->nodes().second->cluster()) continue;

        boundary_edge->add_growth(0.5);

        if (boundary_edge->growth() >= 1) {
            cluster->addEdge(boundary_edge);
            fusion_edges.insert(boundary_edge);
        } else {
            boundary_edges.push_back(boundary_edge);
        }
    }

    cluster->set_boundary(boundary_edges);
    return fusion_edges;
}


void UnionFindDecoder::merge(std::set<std::shared_ptr<DecodingGraphEdge>> fusion_edges, std::vector<std::shared_ptr<Cluster>> &clusters) {
    for (const auto& fusionEdge: fusion_edges) {
        auto nodes = fusionEdge->nodes();
        auto node1 = nodes.first;
        auto node2 = nodes.second;

        if (node1->cluster()->is_neutral()) {
            auto swap = node1;
            node1 = node2;
            node2 = swap;
        }

        auto cluster1 = node1->cluster();
        auto cluster2 = node2->cluster();

        if (cluster1 == cluster2) continue;

        if (cluster1->marked_nodes().size() < cluster2->marked_nodes().size()) {
            auto swap = cluster1;
            cluster1 = cluster2;
            cluster2 = swap;
        }

        for (auto node: cluster2->nodes()) {
            cluster1->addNode(node);
            if (node->marked()) {
                cluster1->addMarkedNode(node);
            }
            node->set_cluster(cluster1);
        }
        for (auto edge: cluster2->edges()) {
            cluster1->addEdge(edge);
        }
        for (auto boundary_edge: cluster2->boundary()) {
            cluster1->addBoundaryEdge(boundary_edge);
        }

        auto cluster_to_be_removed = std::find(clusters.begin(), clusters.end(), cluster2);
        if (cluster_to_be_removed != clusters.end())
            clusters.erase(cluster_to_be_removed);
    }
}