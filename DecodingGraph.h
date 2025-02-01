//
// Created by tommasopeduzzi on 1/8/24.
//

#ifndef CLAYG_DECODINGGRAPH_H
#define CLAYG_DECODINGGRAPH_H

#include <utility>
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


class DecodingGraphNode
{
public:
    enum Type
    {
        ANCILLA,
        VIRTUAL
    };

    struct Id
    {
        Type type;
        int round;
        int id;

        bool operator==(const std::shared_ptr<DecodingGraphNode>::element_type::Id& id) const
        {
            return type == id.type && round == id.round && this->id == id.id;
        };
    };

private:
    Id m_id;

    bool m_marked = false;
    std::optional<std::weak_ptr<Cluster>> m_cluster;
    std::vector<std::weak_ptr<DecodingGraphEdge>> m_edges;

public:
    DecodingGraphNode(Id id) : m_id(id), m_edges({})
    {
    }

    Id id() { return m_id; }

    [[nodiscard]] bool marked() const { return m_marked; }

    void set_marked(const bool marked) { m_marked = marked; }

    std::optional<std::weak_ptr<Cluster>> cluster() { return m_cluster; }

    void set_cluster(const std::optional<std::weak_ptr<Cluster>>& cluster) { m_cluster = cluster; }

    std::vector<std::weak_ptr<DecodingGraphEdge>> edges() { return m_edges; }

    void add_edge(const std::weak_ptr<DecodingGraphEdge>& edge) { m_edges.push_back(edge); }
};

class DecodingGraphEdge
{
public:
    enum Type
    {
        MEASUREMENT,
        NORMAL
    };

    struct Id
    {
        Type type;
        // NOTE: In case that this is a measurement edge, the round indicates the round it originated from
        int round;
        int id;
    };

    struct FusionEdge
    {
        std::shared_ptr<DecodingGraphEdge> edge;
        std::shared_ptr<DecodingGraphNode> tree_node;
        std::shared_ptr<DecodingGraphNode> leaf_node;
    };

private:
    Id m_id;
    std::pair<std::weak_ptr<DecodingGraphNode>, std::weak_ptr<DecodingGraphNode>> m_nodes;
    float m_growth;

public:
    explicit DecodingGraphEdge(const Id id,
                               std::pair<std::weak_ptr<DecodingGraphNode>, std::weak_ptr<DecodingGraphNode>> nodes = {})
        : m_id(id), m_nodes(std::move(nodes)), m_growth(0)
    {
    };

    [[nodiscard]] Id id() const { return m_id; }

    [[nodiscard]] Type type() const { return m_id.type; }

    std::pair<std::weak_ptr<DecodingGraphNode>, std::weak_ptr<DecodingGraphNode>> nodes() { return m_nodes; }

    std::weak_ptr<DecodingGraphNode> other_node(const std::weak_ptr<DecodingGraphNode>& node)
    {
        if (node.lock() == m_nodes.first.lock()) return m_nodes.second;
        if (node.lock() == m_nodes.second.lock()) return m_nodes.first;
        assert(false && "Node not in edge");
    }

    [[nodiscard]] float growth() const { return m_growth; }

    void add_growth(const float growth) { m_growth += growth; }
    void reset_growth() { m_growth = 0; }
};

inline void operator++(DecodingGraphEdge::Id& id, int)
{
    id.id++;
}

class DecodingGraph
{
    int m_ancilla_count_per_layer, D, T;

    std::vector<std::shared_ptr<DecodingGraphNode>> m_nodes;
    std::vector<std::map<int, std::shared_ptr<DecodingGraphNode>>> m_ancilla_nodes;
    std::map<int, std::shared_ptr<DecodingGraphNode>> m_virtual_nodes;
    std::vector<std::shared_ptr<DecodingGraphEdge>> m_edges;
    std::vector<std::map<int, std::shared_ptr<DecodingGraphEdge>>> m_normal_edges;
    std::vector<std::map<int, std::shared_ptr<DecodingGraphEdge>>> m_measurement_edges;
    std::vector<DecodingGraphEdge::Id> m_logical_edges;

public:
    DecodingGraph() : m_ancilla_nodes({}), m_virtual_nodes({}), m_edges({})
    {
    }

    static std::shared_ptr<DecodingGraph> surface_code(int D, int T);
    static std::shared_ptr<DecodingGraph> rotated_surface_code(int D, int T);

    [[nodiscard]] int ancilla_count_per_layer() const { return m_ancilla_count_per_layer; }

    [[nodiscard]] int d() const { return D; }

    [[nodiscard]] int t() const { return T; }

    std::vector<std::shared_ptr<DecodingGraphNode>> nodes() { return m_nodes; }

    std::vector<std::shared_ptr<DecodingGraphEdge>> edges() { return m_edges; }

    void addNode(const std::shared_ptr<DecodingGraphNode>& node);

    std::optional<std::shared_ptr<DecodingGraphNode>> node(DecodingGraphNode::Id id);

    void addEdge(const std::shared_ptr<DecodingGraphEdge>& edge);

    void addLogicalEdge(const std::shared_ptr<DecodingGraphEdge>& edge);

    std::set<int> logical_edge_ids();

    std::optional<std::shared_ptr<DecodingGraphEdge>> edge(DecodingGraphEdge::Id id);

    void reset();
};


#endif //CLAYG_DECODINGGRAPH_H
