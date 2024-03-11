//
// Created by tommasopeduzzi on 1/12/24.
//

#ifndef CLAYG_CLUSTER_H
#define CLAYG_CLUSTER_H

#include <vector>
#include <algorithm>
#include "DecodingGraph.h"

class DecodingGraphNode;
class DecodingGraphEdge;

class Cluster {
    std::shared_ptr<DecodingGraphNode> m_root;
    std::vector<std::shared_ptr<DecodingGraphNode>> m_nodes;
    std::vector<std::shared_ptr<DecodingGraphNode>> m_marked_nodes;
    std::vector<std::shared_ptr<DecodingGraphNode>> m_virtual_nodes;
    std::vector<std::shared_ptr<DecodingGraphEdge>> m_edges;
    std::vector<std::shared_ptr<DecodingGraphEdge>> m_boundary;

public:
    Cluster(std::shared_ptr<DecodingGraphNode> root);

    void addNode(std::shared_ptr<DecodingGraphNode> node) {
        m_nodes.push_back(node);
    }

    void addMarkedNode(std::shared_ptr<DecodingGraphNode> node) {
        m_marked_nodes.push_back(node);
    }

    void addVirtualNode(std::shared_ptr<DecodingGraphNode> node) {
        m_virtual_nodes.push_back(node);
    }

    void removeMarkedNode(std::shared_ptr<DecodingGraphNode> node) {
        m_marked_nodes.erase(std::remove(m_marked_nodes.begin(), m_marked_nodes.end(), node), m_marked_nodes.end());
    }

    void addEdge(std::shared_ptr<DecodingGraphEdge> edge) {
        m_edges.push_back(edge);
    }

    void addBoundaryEdge(std::shared_ptr<DecodingGraphEdge> edge) {
        m_boundary.push_back(edge);
    }

    std::shared_ptr<DecodingGraphNode> root() { return m_root; }

    std::vector<std::shared_ptr<DecodingGraphNode>> nodes() { return m_nodes; }
    std::vector<std::shared_ptr<DecodingGraphNode>> marked_nodes() { return m_marked_nodes; }

    std::vector<std::shared_ptr<DecodingGraphEdge>> edges() { return m_edges; }

    std::vector<std::shared_ptr<DecodingGraphEdge>> boundary() { return m_boundary; }
    void set_boundary(std::vector<std::shared_ptr<DecodingGraphEdge>> boundary) { m_boundary = boundary; }

    bool is_neutral();

    static bool all_clusters_are_neutral(std::vector<std::shared_ptr<Cluster>> clusters);
};




#endif //CLAYG_CLUSTER_H
