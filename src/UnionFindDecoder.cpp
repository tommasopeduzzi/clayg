//
// Created by tommasopeduzzi on 1/21/24.
//

#include <algorithm>

#include "UnionFindDecoder.h"
#include "PeelingDecoder.h"
#include "Logger.h"

using namespace std;

UnionFindDecoder::UnionFindDecoder(const std::unordered_map<std::string, std::string>& args)
    : Decoder(args)
{
    decoder_name_ = "uf";

    if (auto it = args.find("stop_early"); it != args.end()) {
        this->set_stop_early(it->second == "true");
        this->decoder_name_ += it->second == "true" ? "_stop_early" : "_no_stop_early";
    }

    if (const auto it = args.find("growth_policy"); it != args.end()) {
        string policy = it->second;
        if (policy == "third") {
            this->set_growth_policy([] (const DecodingGraphNode::Id start, const DecodingGraphNode::Id end) {
                if (start.round == end.round) return 0.34;
                if (start.round > end.round) return 1.0;
                return 0.5;
            });
            this->decoder_name_ += "_third_growth";
        } else if (policy == "faster_backwards") {
            this->set_growth_policy([] (const DecodingGraphNode::Id start, const DecodingGraphNode::Id end) {
                if (start.round > end.round) return 1.0;
                return 0.5;
            });
            this->decoder_name_ += "_faster_backwards_growth";
        }
    }
}


DecodingResult UnionFindDecoder::decode(const shared_ptr<DecodingGraph> graph)
{
    int consider_up_to_round_ = graph->t();
    if (stop_early_)
    {
        const int buffer_region = (graph->d() + 1) / 2;
        auto marked_nodes_by_round = graph->marked_nodes_by_round();
        int last_round_with_marked_node = 0;
        for (int round = 0; round < graph->t(); round++)
        {
            if (!marked_nodes_by_round[round].empty())
            {
                last_round_with_marked_node = round;
            }
            if (round - last_round_with_marked_node >= buffer_region)
            {
                break;
            }
        }
        consider_up_to_round_ = min(graph->t()-1, last_round_with_marked_node + buffer_region);
    }

    // Initialize clusters
    m_clusters = {};
    for (const auto& node : graph->nodes())
    {
        if (stop_early_ && node->id().round > consider_up_to_round_)
        {
            continue;
        }
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
    logger.log_decoding_step(m_clusters, decoder_name_, steps++, consider_up_to_round_);
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
        logger.log_decoding_step(m_clusters, decoder_name_, steps++, consider_up_to_round_);
        merge(fusion_edges);
        logger.log_decoding_step(m_clusters, decoder_name_, steps++, consider_up_to_round_);
    }
    last_growth_steps_ = steps;
    auto peeling_decoder_results = PeelingDecoder::decode(m_clusters, graph);
    // Log after peeling (no more clusters)
    logger.log_decoding_step({}, decoder_name_, steps++, consider_up_to_round_);
    return {peeling_decoder_results.corrections, consider_up_to_round_};
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
        boundary_edge.growth_from_tree += growth;
        boundary_edges.push_back(boundary_edge);

        if (edge->growth() >= edge->weight())
        {
            fusion_edges.push_back(DecodingGraphEdge::FusionEdge{
                edge,
                tree_node,
                leaf_node
            });
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

            cluster->add_bulk_edge(fusion_edge.edge);
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
            cluster->add_bulk_edge(edge);
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
