//
// Created by tommasopeduzzi on 1/21/24.
//

#include <algorithm>
#include <iostream>
#include "UnionFindDecoder.h"
#include "Logger.h"

#include <fstream>

using namespace std;

DecodingResult UnionFindDecoder::decode(const shared_ptr<DecodingGraph> graph)
{
    // Initialize clusters
    m_clusters = {};
    for (const auto& node : graph->nodes())
    {
        if (node->marked())
        {
            auto cluster = make_shared<Cluster>(node);
            m_clusters.push_back(cluster);
            node->set_cluster(cluster);
            cluster->add_marked_node(node);
        }
    }

    int steps = 0;
    // Main Union Find loop
    logger.log_clusters(m_clusters, "uf", steps++);
    while (!Cluster::all_clusters_are_neutral(m_clusters))
    {
        graph->node(DecodingGraphNode::Id{DecodingGraphNode::ANCILLA, 4, 2});
        vector<DecodingGraphEdge::FusionEdge> fusion_edges;
        for (const auto& cluster : m_clusters)
        {
            auto new_fusion_edges = grow(cluster);
            for (const auto& fusion_edge : new_fusion_edges)
            {
                fusion_edges.push_back(fusion_edge);
            }
        }
        logger.log_clusters(m_clusters, "uf", steps++);
        merge(fusion_edges);
    }
    last_growth_steps_ = steps;
    return PeelingDecoder::decode(m_clusters, graph);
}

vector<DecodingGraphEdge::FusionEdge> UnionFindDecoder::grow(const shared_ptr<Cluster>& cluster)
{
    if (cluster->is_neutral()) return {};
    vector<DecodingGraphEdge::FusionEdge> fusion_edges;
    vector<Cluster::BoundaryEdge> boundary_edges;
    for (const auto& boundary_edge : cluster->boundary())
    {
        // Split node into node that is part of cluster and node that is not (tree and leaf nodes)
        auto tree_node = boundary_edge.tree_node;

        auto leaf_node = boundary_edge.leaf_node;

        auto edge = boundary_edge.edge;
        float growth = growth_policy_(tree_node->id(), leaf_node->id());
        edge->add_growth(growth);

        if (edge->growth() >= edge->weight())
        {
            cluster->add_edge(edge);
            fusion_edges.push_back(DecodingGraphEdge::FusionEdge{
                edge,
                tree_node,
                leaf_node
            });
        }
        else
        {
            boundary_edges.push_back(boundary_edge);
        }
    }

    cluster->set_boundary(boundary_edges);
    return fusion_edges;
}


void UnionFindDecoder::merge(const vector<DecodingGraphEdge::FusionEdge>& fusion_edges)
{
    for (const auto& fusion_edge : fusion_edges)
    {
        auto tree_node = fusion_edge.tree_node;;
        // assert that tree node has cluster
        assert(tree_node->cluster().has_value());
        auto leaf_node = fusion_edge.leaf_node;

        auto cluster = tree_node->cluster().value().lock();
        auto other_cluster_optional = leaf_node->cluster();

        if (!other_cluster_optional.has_value())
        {
            // node is not part of another cluster
            cluster->add_node(leaf_node);
            if (leaf_node->marked())
                cluster->add_marked_node(leaf_node);
            if (leaf_node->id().type == DecodingGraphNode::VIRTUAL)
                cluster->add_virtual_node(leaf_node);

            cluster->add_edge(fusion_edge.edge);
            for (const auto& edge_weak_ptr : leaf_node->edges())
            {
                auto edge = edge_weak_ptr.lock();
                if (edge != fusion_edge.edge)
                {
                    cluster->add_boundary_edge(Cluster::BoundaryEdge{
                        leaf_node,
                        edge->other_node(leaf_node).lock(),
                        edge
                    });
                }
            }
            leaf_node->set_cluster(cluster);
            continue;
        }

        auto other_cluster = other_cluster_optional.value().lock();

        if (other_cluster == cluster)
        {
            continue;
        }

        for (const auto& node : other_cluster->nodes())
        {
            cluster->add_node(node);
            if (node->id().type == DecodingGraphNode::VIRTUAL)
            {
                cluster->add_virtual_node(node);
            }
            if (node->marked())
            {
                cluster->add_marked_node(node);
            }
            node->set_cluster(cluster);
        }
        for (const auto& edge : other_cluster->edges())
        {
            cluster->add_edge(edge);
        }
        for (const auto& boundary : other_cluster->boundary())
        {
            cluster->add_boundary_edge(boundary);
        }

        auto cluster_to_be_removed = find(m_clusters.begin(), m_clusters.end(), other_cluster);
        if (cluster_to_be_removed != m_clusters.end())
            m_clusters.erase(cluster_to_be_removed);
    }
}
