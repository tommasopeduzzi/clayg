//
// Created by tommasopeduzzi on 1/8/24.
//

#include "DecodingGraph.h"

#include <fstream>
#include <iostream>

using namespace std;

shared_ptr<DecodingGraph> DecodingGraph::surface_code(int D, int T) {
    int ancilla_height = D - 1;
    int ancilla_width = D - 1;

    auto graph = make_shared<DecodingGraph>();

    graph->m_ancilla_count_per_layer = ancilla_height * ancilla_width;
    graph->D = D;
    graph->T = T;

    vector<shared_ptr<DecodingGraphNode>> top_boundary_nodes{};
    vector<shared_ptr<DecodingGraphNode>> bottom_boundary_nodes{};

    for (int i = 0; i < ancilla_width; i++) {
        auto top_node = make_shared<DecodingGraphNode>(
                DecodingGraphNode::Id{DecodingGraphNode::Type::VIRTUAL, 0, i});
        auto bottom_node = make_shared<DecodingGraphNode>(
                DecodingGraphNode::Id{DecodingGraphNode::Type::VIRTUAL, 0, i + D - 2});
        graph->addNode(top_node);
        graph->addNode(bottom_node);
        top_boundary_nodes.push_back(top_node);
        bottom_boundary_nodes.push_back(bottom_node);
    }

    for (int t = 0; t < T; t++) {
        DecodingGraphEdge::Id current_edge_id = {DecodingGraphEdge::NORMAL, t, 0};
        for (int y = 0; y < ancilla_height; y++) {
            for (int x = 0; x < ancilla_width; x++) {
                auto id = DecodingGraphNode::Id{DecodingGraphNode::Type::ANCILLA, t, x + y * ancilla_width};
                auto node = make_shared<DecodingGraphNode>(id);
                graph->addNode(node);

                if (t > 0) {
                    auto other_node = DecodingGraphNode::Id{DecodingGraphNode::Type::ANCILLA, t - 1,
                                                            x + y * ancilla_width};
                    auto nodes = make_pair(node, graph->node(other_node).value());
                    auto edge = make_shared<DecodingGraphEdge>(DecodingGraphEdge::Id{DecodingGraphEdge::MEASUREMENT, t - 1, x + y * ancilla_width}, nodes);
                    graph->addEdge(edge);
                }

                if (y == 0) {
                    auto nodes = make_pair(node, top_boundary_nodes[x]);
                    graph->addEdge(make_shared<DecodingGraphEdge>(
                            current_edge_id, nodes));
                    current_edge_id++;
                } else {
                    auto other_node = DecodingGraphNode::Id{DecodingGraphNode::Type::ANCILLA, 0,
                                                            x + (y - 1) * ancilla_width};
                    auto nodes = make_pair(node, graph->node(other_node).value());
                    graph->addEdge(make_shared<DecodingGraphEdge>(
                            current_edge_id, nodes));
                    current_edge_id++;
                }

                if (x > 0) {
                    auto other_node = DecodingGraphNode::Id{DecodingGraphNode::Type::ANCILLA, 0,
                                                            x - 1 + y * ancilla_width};
                    auto nodes = make_pair(node, graph->node(other_node).value());
                    auto edge = make_shared<DecodingGraphEdge>(current_edge_id, nodes);
                    current_edge_id++;
                    graph->addEdge(edge);
                    if (y == 0) {
                        graph->addLogicalEdge(edge);
                    }
                }

                if (y == ancilla_height - 1) {
                    auto nodes = make_pair(node, bottom_boundary_nodes[x]);
                    graph->addEdge(make_shared<DecodingGraphEdge>(
                            current_edge_id, nodes));
                    current_edge_id++;
                }
            }
        }
    }

    return graph;
}

shared_ptr<DecodingGraph> DecodingGraph::rotated_surface_code(int D, int T) {
    int ancilla_height = D - 1;
    int ancilla_width = static_cast<int>(D / 2) + 1;

    auto graph = make_shared<DecodingGraph>();

    graph->m_ancilla_count_per_layer = ancilla_height * ancilla_width;
    graph->D = D;
    graph->T = T;

    vector<shared_ptr<DecodingGraphNode>> top_boundary_nodes{};
    vector<shared_ptr<DecodingGraphNode>> bottom_boundary_nodes{};

    for (int i = 0; i < ancilla_width; i++) {
        auto top_node = make_shared<DecodingGraphNode>(
                DecodingGraphNode::Id{DecodingGraphNode::Type::VIRTUAL, 0, i});
        auto bottom_node = make_shared<DecodingGraphNode>(
                DecodingGraphNode::Id{DecodingGraphNode::Type::VIRTUAL, 0, i + ancilla_width});
        graph->addNode(top_node);
        graph->addNode(bottom_node);
        top_boundary_nodes.push_back(top_node);
        bottom_boundary_nodes.push_back(bottom_node);
    }

    for (int t = 0; t < T; t++) {
        pair<shared_ptr<DecodingGraphNode>, shared_ptr<DecodingGraphNode>> nodes{};
        DecodingGraphEdge::Id current_edge_id{DecodingGraphEdge::NORMAL, t, 0};
        for (int y = 0; y < ancilla_height; y++) {
            for (int x = 0; x < ancilla_width; x++) {
                auto id = DecodingGraphNode::Id{DecodingGraphNode::Type::ANCILLA, t, x + y * ancilla_width};
                shared_ptr<DecodingGraphNode> node = make_shared<DecodingGraphNode>(id);
                graph->addNode(node);

                // Connect to previous set of ancillas
                if (t > 0) {
                    auto edge_id = DecodingGraphEdge::Id{DecodingGraphEdge::MEASUREMENT, t - 1, x + y * ancilla_width};
                    auto prev_id = DecodingGraphNode::Id{DecodingGraphNode::ANCILLA, t - 1, x + y * ancilla_width};
                    nodes = make_pair(node, graph->node(prev_id).value());
                    graph->addEdge(make_shared<DecodingGraphEdge>(edge_id, nodes));
                }

                // Connect to top boundary
                if (y == 0) {
                    nodes = make_pair(node, top_boundary_nodes[x]);
                    graph->addEdge(make_shared<DecodingGraphEdge>(current_edge_id, nodes));
                    current_edge_id++;
                    if (x + 1 < ancilla_width) {
                        nodes = make_pair(node, top_boundary_nodes[x + 1]);
                        graph->addEdge(make_shared<DecodingGraphEdge>(current_edge_id, nodes));
                        current_edge_id++;
                    }
                    continue;
                }
                if (y % 2 == 0) {
                    auto other_node_id = DecodingGraphNode::Id{DecodingGraphNode::ANCILLA, t,
                                                               x + (y - 1) * ancilla_width};
                    nodes = make_pair(node, graph->node(other_node_id).value());
                    graph->addEdge(make_shared<DecodingGraphEdge>(current_edge_id, nodes));
                    current_edge_id++;
                    if (x + 1 < ancilla_width) {
                        other_node_id = DecodingGraphNode::Id{DecodingGraphNode::ANCILLA, t,
                                                              x + (y - 1) * ancilla_width + 1};
                        nodes = make_pair(node, graph->node(other_node_id).value());
                        graph->addEdge(make_shared<DecodingGraphEdge>(current_edge_id, nodes));
                        current_edge_id++;
                    }
                } else {
                    if (x - 1 >= 0) {
                        auto other_node_id = DecodingGraphNode::Id{DecodingGraphNode::ANCILLA, t,
                                                                   x + (y - 1) * ancilla_width - 1};
                        nodes = make_pair(node, graph->node(other_node_id).value());
                        auto edge = make_shared<DecodingGraphEdge>(current_edge_id, nodes);
                        graph->addEdge(edge);
                        current_edge_id++;
                        if (y == 1)
                            graph->addLogicalEdge(edge);
                    }
                    auto other_node_id = DecodingGraphNode::Id{DecodingGraphNode::ANCILLA, t,
                                                               x + (y - 1) * ancilla_width};
                    nodes = make_pair(node, graph->node(other_node_id).value());
                    auto edge = make_shared<DecodingGraphEdge>(current_edge_id, nodes);
                    graph->addEdge(edge);
                    current_edge_id++;
                    if (y == 1)
                        graph->addLogicalEdge(edge);
                }

            }
        }
        for (int x = 0; x < ancilla_width; x++) {
            auto id = DecodingGraphNode::Id{DecodingGraphNode::ANCILLA, t, x + ancilla_width * (ancilla_height - 1)};
            auto node = graph->node(id).value();
            if (x - 1 >= 0) {
                graph->addEdge(make_shared<DecodingGraphEdge>(current_edge_id,
                                                                  make_pair(node, bottom_boundary_nodes[x - 1])));
                current_edge_id++;
            }
            graph->addEdge(make_shared<DecodingGraphEdge>(current_edge_id,
                                                              make_pair(node, bottom_boundary_nodes[x])));
            current_edge_id++;

        }
    }

    return graph;
}

optional<shared_ptr<DecodingGraphNode>> DecodingGraph::node(const DecodingGraphNode::Id id) {
    if (id.type == DecodingGraphNode::Type::VIRTUAL) {
        return m_virtual_nodes[id.id];
    }
    if (id.type == DecodingGraphNode::Type::ANCILLA) {
        if (m_ancilla_nodes.size() <= id.round)
            return {};
        return m_ancilla_nodes[id.round][id.id];
    }
    return {};
}

optional<shared_ptr<DecodingGraphEdge>> DecodingGraph::edge(const DecodingGraphEdge::Id id) {
    if (id.type == DecodingGraphEdge::MEASUREMENT) {
        if (m_measurement_edges.size() <= id.round)
            return {};
        return m_measurement_edges[id.round][id.id];
    }
    if (id.type == DecodingGraphEdge::NORMAL) {
        if (m_normal_edges.size() <= id.round)
            return {};
        return m_normal_edges[id.round][id.id];
    }
    return {};
}

void DecodingGraph::addEdge(const shared_ptr<DecodingGraphEdge>& edge) {
    m_edges.push_back(edge);
    auto [type, round, id] = edge->id();
    if (type == DecodingGraphEdge::NORMAL) {
        if (m_normal_edges.size() <= round) {
            m_normal_edges.resize(round + 1);
        }
        m_normal_edges[round][id] = edge;
    } else if (type == DecodingGraphEdge::MEASUREMENT) {
        if (m_measurement_edges.size() <= round) {
            m_measurement_edges.resize(round + 1);
        }
        m_measurement_edges[round][id] = edge;
    }
    edge->nodes().first.lock()->add_edge(edge);
    edge->nodes().second.lock()->add_edge(edge);
}

void DecodingGraph::addLogicalEdge(const shared_ptr<DecodingGraphEdge>& edge) {
    m_logical_edges.push_back(edge->id());
}

void DecodingGraph::addNode(const shared_ptr<DecodingGraphNode>& node) {
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

set<int> DecodingGraph::logical_edge_ids() {
    set<int> ids;
    for (auto edge: m_logical_edges) {
        ids.insert(edge.id);
    }
    return ids;
}

void DecodingGraph::reset() {
    for (const auto& node: m_nodes) {
        node->set_cluster(nullopt);
        node->set_marked(false);
    }
    for (const auto& edge: m_edges) {
        edge->reset_growth();
    }
}

void DecodingGraph::mark(const std::vector<std::shared_ptr<DecodingGraphEdge>>& error_edges)
{
    for (const auto& edge : error_edges)
    {
        auto nodes = {edge->nodes().first, edge->nodes().second};
        for (const auto& node_weak_ptr : nodes)
        {
            auto node = node_weak_ptr.lock();
            if (node->id().type == DecodingGraphNode::VIRTUAL)
            {
                continue;
            }
            node->set_marked(!node->marked());
        }
    }
}

void DecodingGraph::dump(const string& filename) {
    ofstream file(filename);
    if (!file) {
        // print out the error
        cerr << "Error opening file, while dumping decoding graph!" << endl;
        return;
    }

    for (const auto& edge : m_edges) {
        auto [node1, node2] = edge->nodes();
        if (auto n1 = node1.lock(), n2 = node2.lock(); n1 && n2) {
            file << n1->id().type << "-" << n1->id().round << "-" << n1->id().id << ","
                 << n2->id().type << "-" << n2->id().round << "-" << n2->id().id << ","
                 << edge->id().type << "-" << edge->id().round << "-" << edge->id().id << "\n";
        }
    }

    file.close();
}

