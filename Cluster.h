//
// Created by tommasopeduzzi on 1/12/24.
//

#ifndef CLAYG_CLUSTER_H
#define CLAYG_CLUSTER_H

#include <utility>
#include <vector>
#include <algorithm>
#include "DecodingGraph.h"

class DecodingGraphNode;
class DecodingGraphEdge;

class Cluster
{
public:
    struct BoundaryEdge
    {
        std::shared_ptr<DecodingGraphNode> tree_node;
        std::shared_ptr<DecodingGraphNode> leaf_node;
        std::shared_ptr<DecodingGraphEdge> edge;
    };

private:
    std::shared_ptr<DecodingGraphNode> m_root;
    std::vector<std::shared_ptr<DecodingGraphNode>> m_nodes;
    std::vector<std::shared_ptr<DecodingGraphNode>> m_marked_nodes;
    std::vector<std::shared_ptr<DecodingGraphNode>> m_virtual_nodes;
    std::vector<std::shared_ptr<DecodingGraphEdge>> m_edges;
    std::vector<BoundaryEdge> m_boundary;

public:
    explicit Cluster(std::shared_ptr<DecodingGraphNode> root);

    void add_node(const std::shared_ptr<DecodingGraphNode>& node)
    {
        m_nodes.push_back(node);
    }

    void add_marked_node(const std::shared_ptr<DecodingGraphNode>& node)
    {
        m_marked_nodes.push_back(node);
    }

    void add_virtual_node(const std::shared_ptr<DecodingGraphNode>& node)
    {
        m_virtual_nodes.push_back(node);
    }

    void remove_marked_node(const std::shared_ptr<DecodingGraphNode>& node)
    {
        m_marked_nodes.erase(std::remove(m_marked_nodes.begin(), m_marked_nodes.end(), node), m_marked_nodes.end());
    }

    void add_edge(const std::shared_ptr<DecodingGraphEdge>& edge)
    {
        m_edges.push_back(edge);
    }

    void add_boundary_edge(const BoundaryEdge& boundary_edge)
    {
        m_boundary.push_back(boundary_edge);
    }

    std::weak_ptr<DecodingGraphNode> root() { return m_root; }

    std::vector<std::shared_ptr<DecodingGraphNode>> nodes() { return m_nodes; }
    std::vector<std::shared_ptr<DecodingGraphNode>> marked_nodes() { return m_marked_nodes; }

    std::vector<std::shared_ptr<DecodingGraphEdge>> edges() { return m_edges; }

    std::vector<BoundaryEdge> boundary() { return m_boundary; }
    void set_boundary(const std::vector<BoundaryEdge>& boundary) { m_boundary = boundary; }

    bool is_neutral(bool consider_virtual_nodes = true) const;

    static bool all_clusters_are_neutral(std::vector<std::shared_ptr<Cluster>> clusters,
                                         bool consider_virtual_nodes = true);
};


#endif //CLAYG_CLUSTER_H
