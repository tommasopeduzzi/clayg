//
// Created by tommasopeduzzi on 1/28/24.
//

#include "ClAYGDecoder.h"

#include <cmath>
#include <iostream>

#include "Logger.h"

using namespace std;

DecodingResult ClAYGDecoder::decode(shared_ptr<DecodingGraph> graph)
{
    vector<shared_ptr<DecodingGraphNode>> marked_nodes;

    for (const auto& node : graph->nodes())
    {
        if (node->marked())
        {
            marked_nodes.push_back(node);
        }
    }

    sort(marked_nodes.begin(), marked_nodes.end(), [](const auto& node1, const auto& node2)
    {
        if (node1->id().round == node2->id().round)
        {
            return node1->id().id < node2->id().id;
        }
        return node1->id().round < node2->id().round;
    });

    const int rounds = graph->t();
    if (!decoding_graph_ || decoding_graph_->d() != graph->d())
    {
        decoding_graph_ = DecodingGraph::rotated_surface_code(graph->d(), graph->t());
    }
    else
    {
        decoding_graph_->reset(); // Reset the graph to its initial state
    }
    vector<shared_ptr<DecodingGraphEdge>> error_edges;

    m_clusters = {};
    int step = 0;
    int considered_up_to_round = rounds - 1;
    last_growth_steps_ = -(rounds-1); // don't count last round as being negative
    for (int round = 0; round < rounds; round++)
    {
        last_growth_steps_ = ceil(last_growth_steps_);
        vector<shared_ptr<DecodingGraphNode>> nodes;
        for (const auto& node : marked_nodes)
        {
            if (node->id().round == round && node->marked())
            {
                nodes.push_back(node);
            }
        }
        int added_nodes = 0;
        for (const auto& node : nodes)
        {
            add(decoding_graph_, node);
            added_nodes++;
        }
        auto new_error_edges = clean(decoding_graph_);
        logger.log_clusters(m_clusters, decoder_name_, step++);
        error_edges.insert(error_edges.end(), new_error_edges.begin(), new_error_edges.end());
        for (int growth_round = 0; growth_round < growth_rounds_; growth_round++)
        {
            vector<DecodingGraphEdge::FusionEdge> fusion_edges;
            for (const auto& cluster : m_clusters)
            {
                if (cluster->is_neutral()) continue;
                auto new_fusion_edges = grow(cluster);
                for (const auto& fusion_edge : new_fusion_edges)
                {
                    fusion_edges.push_back(fusion_edge);
                }
            }
            if (Logger::instance().is_dump_enabled())
            {
                logger.log_clusters(m_clusters, decoder_name_, step++);
            }
            merge(fusion_edges);
            if (stop_early_ && Cluster::all_clusters_are_neutral(m_clusters))
            {
                break;
            }
            // FIXME: The last round is accounted for as a 'real' step
            last_growth_steps_ = last_growth_steps_ + (round == rounds-1 ? 1 :  1.0/growth_rounds_);
        }
        new_error_edges = clean(decoding_graph_);
        error_edges.insert(error_edges.end(), new_error_edges.begin(), new_error_edges.end());
        if (stop_early_ && Cluster::all_clusters_are_neutral(m_clusters))
        {
            considered_up_to_round = round;
            break;
        }
    }

    while (!Cluster::all_clusters_are_neutral(m_clusters))
    {
        last_growth_steps_ += 1;
        vector<DecodingGraphEdge::FusionEdge> fusion_edges;
        for (const auto& cluster : m_clusters)
        {
            if (cluster->is_neutral()) continue;
            auto new_fusion_edges = grow(cluster);
            for (const auto& fusion_edge : new_fusion_edges)
            {
                fusion_edges.push_back(fusion_edge);
            }
        }
        logger.log_clusters(m_clusters, decoder_name_, step++);
        merge(fusion_edges);
    }

    auto new_error_edges = clean(decoding_graph_);
    error_edges.insert(error_edges.end(), new_error_edges.begin(), new_error_edges.end());

    return {error_edges, considered_up_to_round};
}

void ClAYGDecoder::add(const shared_ptr<DecodingGraph>& graph, shared_ptr<DecodingGraphNode> node)
{
    // Find corresponding node in the flattened decoding graph
    auto id = node->id();
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

vector<shared_ptr<DecodingGraphEdge>> ClAYGDecoder::clean(const shared_ptr<DecodingGraph>& decoding_graph)
{
    vector<shared_ptr<DecodingGraphEdge>> error_edges;
    vector<shared_ptr<Cluster>> new_clusters;
    for (auto& cluster : m_clusters)
    {
        if (!cluster->is_neutral())
        {
            new_clusters.push_back(move(cluster));
            continue;
        }

        auto new_error_edges = PeelingDecoder::peel(cluster, decoding_graph);
        error_edges.insert(error_edges.end(), new_error_edges.begin(), new_error_edges.end());

        for (const auto& node : cluster->nodes())
        {
            node->set_cluster(nullopt);
        }
        for (const auto& edge : cluster->edges())
        {
            edge->reset_growth();
        }

        for (const auto& boundary_edge : cluster->boundary())
        {
            boundary_edge.edge->reset_growth();
        }
    }
    m_clusters = move(new_clusters);
    return error_edges;
}

DecodingResult SingleLayerClAYGDecoder::decode(shared_ptr<DecodingGraph> graph)
{
    vector<shared_ptr<DecodingGraphNode>> marked_nodes;

    for (const auto& node : graph->nodes())
    {
        if (node->marked())
        {
            marked_nodes.push_back(node);
        }
    }

    sort(marked_nodes.begin(), marked_nodes.end(), [](const auto& node1, const auto& node2)
    {
        if (node1->id().round == node2->id().round)
        {
            return node1->id().id < node2->id().id;
        }
        return node1->id().round < node2->id().round;
    });

    const int rounds = graph->t();
    if (decoding_graph_->d() != graph->d())
    {
        decoding_graph_ = DecodingGraph::rotated_surface_code(graph->d(), 1);
    }
    else
    {
        decoding_graph_->reset(); // Reset the graph to its initial state
    }

    vector<shared_ptr<DecodingGraphEdge>> error_edges;

    m_clusters = {};
    int step = 0;
    int considered_up_to_round = rounds - 1;
    last_growth_steps_ = -(rounds-1);
    for (int round = 0; round < rounds; round++)
    {
        last_growth_steps_ = ceil(last_growth_steps_);
        vector<shared_ptr<DecodingGraphNode>> nodes;
        for (const auto& node : marked_nodes)
        {
            if (node->id().round == round && node->marked())
            {
                nodes.push_back(node);
            }
        }
        int added_nodes = 0;
        for (const auto& node : nodes)
        {
            add(decoding_graph_, node);
            added_nodes++;
        }
        auto new_error_edges = clean(decoding_graph_);
        logger.log_clusters(m_clusters, decoder_name_, step++);
        error_edges.insert(error_edges.end(), new_error_edges.begin(), new_error_edges.end());
        for (int growth_round = 0; growth_round < growth_rounds_; growth_round++)
        {
            vector<DecodingGraphEdge::FusionEdge> fusion_edges;
            for (const auto& cluster : m_clusters)
            {
                if (cluster->is_neutral()) continue;
                auto new_fusion_edges = grow(cluster);
                for (const auto& fusion_edge : new_fusion_edges)
                {
                    fusion_edges.push_back(fusion_edge);
                }
            }
            if (Logger::instance().is_dump_enabled())
            {
                logger.log_clusters(m_clusters, decoder_name_, step++);
            }
            merge(fusion_edges);
            if (stop_early_ && Cluster::all_clusters_are_neutral(m_clusters))
            {
                break;
            }
            last_growth_steps_ = last_growth_steps_ + (round == rounds-1 ? 1 :  1.0/growth_rounds_);
        }
        new_error_edges = clean(decoding_graph_);
        error_edges.insert(error_edges.end(), new_error_edges.begin(), new_error_edges.end());
        if (stop_early_ && Cluster::all_clusters_are_neutral(m_clusters))
        {
            considered_up_to_round = round;
            break;
        }
    }

    while (!Cluster::all_clusters_are_neutral(m_clusters))
    {
        last_growth_steps_++;
        vector<DecodingGraphEdge::FusionEdge> fusion_edges;
        for (const auto& cluster : m_clusters)
        {
            if (cluster->is_neutral()) continue;
            auto new_fusion_edges = grow(cluster);
            for (const auto& fusion_edge : new_fusion_edges)
            {
                fusion_edges.push_back(fusion_edge);
            }
        }
        logger.log_clusters(m_clusters, decoder_name_, step++);
        merge(fusion_edges);
    }

    auto new_error_edges = clean(decoding_graph_);
    error_edges.insert(error_edges.end(), new_error_edges.begin(), new_error_edges.end());

    return {error_edges, considered_up_to_round};
}

void SingleLayerClAYGDecoder::add(const shared_ptr<DecodingGraph>& graph, shared_ptr<DecodingGraphNode> node)
{
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
