//
// Created by tommasopeduzzi on 1/8/24.
//

#include "DecodingGraph.h"

DecodingGraph DecodingGraph::rotated_surface_code(int D, int T) {
    int ancilla_height = D - 1;
    int ancilla_width = int(D / 2) + 1;

    DecodingGraph graph;

    graph.m_ancilla_count_per_layer = ancilla_height * ancilla_width;
    graph.D = D;
    graph.T = T;

    std::vector<std::shared_ptr<DecodingGraphNode>> top_boundary_nodes{};
    std::vector<std::shared_ptr<DecodingGraphNode>> bottom_boundary_nodes{};

    for (int i = 0; i < ancilla_width; i++) {
        auto top_node = std::make_shared<DecodingGraphNode>(
                DecodingGraphNode::Id{DecodingGraphNode::Type::VIRTUAL, 0, i});
        auto bottom_node = std::make_shared<DecodingGraphNode>(
                DecodingGraphNode::Id{DecodingGraphNode::Type::VIRTUAL, 0, i + D - 2});
        graph.addNode(top_node);
        graph.addNode(bottom_node);
        top_boundary_nodes.push_back(top_node);
        bottom_boundary_nodes.push_back(bottom_node);
    }

    for (int t = 0; t < T; t++) {
        std::pair<std::shared_ptr<DecodingGraphNode>, std::shared_ptr<DecodingGraphNode>> nodes{};
        DecodingGraphEdge::Id current_edge_id{DecodingGraphEdge::NORMAL, t, 0};
        for (int y = 0; y < ancilla_height; y++) {
            for (int x = 0; x < ancilla_width; x++) {
                auto id = DecodingGraphNode::Id{DecodingGraphNode::Type::ANCILLA, t, x + y * ancilla_width};
                std::shared_ptr<DecodingGraphNode> node = std::make_shared<DecodingGraphNode>(id);
                graph.addNode(node);

                // Connect to previous set of ancillas
                if (t > 0) {
                    auto edge_id = DecodingGraphEdge::Id{DecodingGraphEdge::MEASUREMENT, t - 1, x + y * ancilla_width};
                    auto prev_id = DecodingGraphNode::Id{DecodingGraphNode::ANCILLA, t - 1, x + y * ancilla_width};
                    nodes = std::make_pair(node, graph.node(prev_id).value());
                    graph.addEdge(std::make_shared<DecodingGraphEdge>(edge_id, nodes));
                }

                // Connect to top boundary
                if (y == 0) {
                    nodes = std::make_pair(node, top_boundary_nodes[x]);
                    graph.addEdge(std::make_shared<DecodingGraphEdge>(current_edge_id, nodes));
                    current_edge_id++;
                    if (x + 1 < ancilla_width) {
                        nodes = std::make_pair(node, top_boundary_nodes[x + 1]);
                        graph.addEdge(std::make_shared<DecodingGraphEdge>(current_edge_id, nodes));
                        current_edge_id++;
                    }
                    continue;
                }
                if (y % 2 == 0) {
                    auto other_node_id = DecodingGraphNode::Id{DecodingGraphNode::ANCILLA, t,
                                                               x + (y - 1) * ancilla_width};
                    nodes = std::make_pair(node, graph.node(other_node_id).value());
                    graph.addEdge(std::make_shared<DecodingGraphEdge>(current_edge_id, nodes));
                    current_edge_id++;
                    if (x + 1 < ancilla_width) {
                        other_node_id = DecodingGraphNode::Id{DecodingGraphNode::ANCILLA, t,
                                                              x + (y - 1) * ancilla_width + 1};
                        nodes = std::make_pair(node, graph.node(other_node_id).value());
                        graph.addEdge(std::make_shared<DecodingGraphEdge>(current_edge_id, nodes));
                        current_edge_id++;
                    }
                } else {
                    if (x - 1 >= 0) {
                        auto other_node_id = DecodingGraphNode::Id{DecodingGraphNode::ANCILLA, t,
                                                                   x + (y - 1) * ancilla_width - 1};
                        nodes = std::make_pair(node, graph.node(other_node_id).value());
                        graph.addEdge(std::make_shared<DecodingGraphEdge>(current_edge_id, nodes));
                        current_edge_id++;
                    }
                    auto other_node_id = DecodingGraphNode::Id{DecodingGraphNode::ANCILLA, t,
                                                               x + (y - 1) * ancilla_width};
                    nodes = std::make_pair(node, graph.node(other_node_id).value());
                    graph.addEdge(std::make_shared<DecodingGraphEdge>(current_edge_id, nodes));
                    current_edge_id++;
                }

            }
        }
        for (int x = 0; x < ancilla_width; x++) {
            auto id = DecodingGraphNode::Id{DecodingGraphNode::ANCILLA, t, x + ancilla_width * (ancilla_height - 1)};
            auto node = graph.node(id).value();
            if (x - 1 >= 0) {
                graph.addEdge(std::make_shared<DecodingGraphEdge>(current_edge_id,
                                                                  std::make_pair(node, bottom_boundary_nodes[x - 1])));
                current_edge_id++;
            }
            graph.addEdge(std::make_shared<DecodingGraphEdge>(current_edge_id,
                                                              std::make_pair(node, bottom_boundary_nodes[x])));
            current_edge_id++;

        }
    }

    for (auto node: graph.m_nodes) {
        auto cluster = std::make_shared<Cluster>(node);
        node->set_cluster(cluster);
    }

    return graph;
}

std::optional<std::shared_ptr<DecodingGraphNode>> DecodingGraph::node(DecodingGraphNode::Id id) {
    if (id.type == DecodingGraphNode::Type::VIRTUAL) {
        return m_virtual_nodes[id.id];
    } else if (id.type == DecodingGraphNode::Type::ANCILLA) {
        if (m_ancilla_nodes.size() <= id.round)
            return {};
        return m_ancilla_nodes[id.round][id.id];
    }
    return {};
}

std::optional<std::shared_ptr<DecodingGraphEdge>> DecodingGraph::edge(DecodingGraphEdge::Id id) {
    if (id.type == DecodingGraphEdge::MEASUREMENT) {
        if (m_measurement_edges.size() <= id.round)
            return {};
        return m_measurement_edges[id.round][id.id];
    } else if (id.type == DecodingGraphEdge::NORMAL) {
        if (m_normal_edges.size() <= id.round)
            return {};
        return m_normal_edges[id.round][id.id];
    }
}

void DecodingGraph::addEdge(std::shared_ptr<DecodingGraphEdge> edge) {
    m_edges.push_back(edge);
    auto id = edge->id();
    if (id.type == DecodingGraphEdge::NORMAL) {
        if (m_normal_edges.size() <= id.round) {
            m_normal_edges.resize(id.round + 1);
        }
        m_normal_edges[id.round][id.id] = edge;
    } else if (id.type == DecodingGraphEdge::MEASUREMENT) {
        if (m_measurement_edges.size() <= id.round) {
            m_measurement_edges.resize(id.round + 1);
        }
        m_measurement_edges[id.round][id.id] = edge;
    }
    edge->nodes().first->addEdge(edge);
    edge->nodes().second->addEdge(edge);
}

void DecodingGraph::addNode(std::shared_ptr<DecodingGraphNode> node) {
    m_nodes.push_back(node);
    auto id = node->id();
    if (id.type == DecodingGraphNode::Type::ANCILLA) {
        if (m_ancilla_nodes.size() <= id.round) {
            m_ancilla_nodes.resize(id.round + 1);
        }
        m_ancilla_nodes[id.round][id.id] = node;
    } else {
        m_virtual_nodes[id.id] = node;
    }
}