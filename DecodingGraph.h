//
// Created by tommasopeduzzi on 1/8/24.
//

#ifndef CLAYG_DECODINGGRAPH_H
#define CLAYG_DECODINGGRAPH_H

#include <vector>
#include <optional>
#include <memory>
#include <map>
#include <set>
#include <cassert>
#include "Cluster.h"

class Cluster;

class DecodingGraphEdge;

class DecodingGraphNode;


class DecodingGraphNode {
public:
    enum Type {
        ANCILLA,
        VIRTUAL
    };
    struct Id {
        Type type;
        int round;
        int id;
    };

private:
    Id m_id;

    bool m_marked = false;
    std::shared_ptr<Cluster> m_cluster;
    std::vector<std::shared_ptr<DecodingGraphEdge>> m_edges;

public:
    DecodingGraphNode(Id id) : m_id(id), m_edges({}) {}

    Id id() { return m_id; }

    bool marked() { return m_marked; }

    void set_marked(bool marked) { m_marked = marked; }

    std::shared_ptr<Cluster> cluster() { return m_cluster; }

    void set_cluster(std::shared_ptr<Cluster> cluster) { m_cluster = cluster; }

    std::vector<std::shared_ptr<DecodingGraphEdge>> edges() { return m_edges; }

    void addEdge(std::shared_ptr<DecodingGraphEdge> edge) { m_edges.push_back(edge); }
};

class DecodingGraphEdge {
public:
    enum Type {
        MEASUREMENT,
        NORMAL
    };
    struct Id {
        Type type;
        // NOTE: In case that this is a measurement edge, the round indicates the round it originated from
        int round;
        int id;
    };

private:
    Id m_id;
    std::pair<std::shared_ptr<DecodingGraphNode>, std::shared_ptr<DecodingGraphNode>> m_nodes;
    float m_growth;

public:
    DecodingGraphEdge(Id id,
                      std::pair<std::shared_ptr<DecodingGraphNode>, std::shared_ptr<DecodingGraphNode>> nodes = {})
            : m_id(id), m_nodes(nodes), m_growth(0) {};

    Id id() { return m_id; }

    Type type() { return m_id.type; }

    std::pair<std::shared_ptr<DecodingGraphNode>, std::shared_ptr<DecodingGraphNode>> nodes() { return m_nodes; }

    std::shared_ptr<DecodingGraphNode> other_node(std::shared_ptr<DecodingGraphNode> node) {
        if (node == m_nodes.first) return m_nodes.second;
        if (node == m_nodes.second) return m_nodes.first;
        assert(false && "Node not in edge");
    }

    float growth() { return m_growth; }

    void add_growth(float growth) { m_growth += growth; }
};

inline void operator++(DecodingGraphEdge::Id &id, int) {
    id.id++;
}

class DecodingGraph {
    int m_ancilla_count_per_layer, D, T;

    std::vector<std::shared_ptr<DecodingGraphNode>> m_nodes;
    std::vector<std::map<int, std::shared_ptr<DecodingGraphNode>>> m_ancilla_nodes;
    std::map<int, std::shared_ptr<DecodingGraphNode>> m_virtual_nodes;
    std::vector<std::shared_ptr<DecodingGraphEdge>> m_edges;
    std::vector<std::map<int, std::shared_ptr<DecodingGraphEdge>>> m_normal_edges;
    std::vector<std::map<int, std::shared_ptr<DecodingGraphEdge>>> m_measurement_edges;
    std::vector<DecodingGraphEdge::Id> m_logical_edges;

public:
    DecodingGraph() : m_ancilla_nodes({}), m_virtual_nodes({}), m_edges({}) {}

    static std::shared_ptr<DecodingGraph> surface_code(int D, int T);
    static std::shared_ptr<DecodingGraph> rotated_surface_code(int D, int T);

    const int ancilla_count_per_layer() { return m_ancilla_count_per_layer; }

    const int d() { return D; }

    const int t() { return T; }

    std::vector<std::shared_ptr<DecodingGraphNode>> nodes() { return m_nodes; }

    std::vector<std::shared_ptr<DecodingGraphEdge>> edges() { return m_edges; }

    void addNode(std::shared_ptr<DecodingGraphNode> node);

    std::optional<std::shared_ptr<DecodingGraphNode>> node(DecodingGraphNode::Id id);

    void addEdge(std::shared_ptr<DecodingGraphEdge> edge);

    void addLogicalEdge(std::shared_ptr<DecodingGraphEdge> edge);

    std::set<int> logical_edge_ids();

    std::optional<std::shared_ptr<DecodingGraphEdge>> edge(DecodingGraphEdge::Id id);

    void reset();
};


#endif //CLAYG_DECODINGGRAPH_H
