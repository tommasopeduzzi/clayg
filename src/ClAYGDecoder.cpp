//
// Created by tommasopeduzzi on 1/28/24.
//

#include <cmath>

#include "ClAYGDecoder.h"
#include "Logger.h"
#include "PeelingDecoder.h"

using namespace std;

ClAYGDecoder::ClAYGDecoder(std::unordered_map<std::string, std::string> args)
{
    this->decoder_name_ = "clayg";

    if (auto it = args.find("stop_early"); it != args.end()) {
        this->set_stop_early(it->second == "true");
        this->decoder_name_ += it->second == "true" ? "_stop_early" : "_no_stop_early";
    }

    if (auto it = args.find("growth_policy"); it != args.end()) {
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

    if (auto it = args.find("cluster_lifetime"); it != args.end()) {
        this->set_cluster_lifetime_factor(stod(it->second));
        this->decoder_name_ += "_lifetime_" + it->second;
    }
}

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
    int last_encountered_non_neutral_cluster = 0;
    last_growth_steps_ = -(rounds-1); // don't count last round as being negative
    for (current_round_ = 0; current_round_ < rounds; current_round_++)
    {
        last_growth_steps_ = ceil(last_growth_steps_);
        vector<shared_ptr<DecodingGraphNode>> nodes;
        for (const auto& node : marked_nodes)
        {
            if (node->id().round == current_round_ && node->marked())
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
        // Growth after adding last round belongs to the bulk growth
        if (current_round_ == rounds-1)
            break;
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
            logger.log_clusters(m_clusters, decoder_name_, step++);
            merge(fusion_edges);
            last_growth_steps_ += (1.0/growth_rounds_);
            if (stop_early_ && Cluster::all_clusters_are_neutral(m_clusters))
            {
                break;
            }
        }
        logger.log_clusters(m_clusters, decoder_name_, step++);
        new_error_edges = clean(decoding_graph_);
        error_edges.insert(error_edges.end(), new_error_edges.begin(), new_error_edges.end());
        if (stop_early_ && Cluster::all_clusters_are_neutral(m_clusters))
        {
            if (current_round_-last_encountered_non_neutral_cluster >= (decoding_graph_->d()-1)/2)
            {
                considered_up_to_round = current_round_;
                break;
            }
        }
        else
        {
            last_encountered_non_neutral_cluster = current_round_;
        }
    }

    logger.log_clusters(m_clusters, decoder_name_, step++);

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

    auto peeling_result = PeelingDecoder::decode(m_clusters, decoding_graph_);
    auto new_error_edges = peeling_result.corrections;
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
        // Keep non-neutral-clusters around
        if (!cluster->is_neutral())
        {
            new_clusters.push_back(move(cluster));
            continue;
        }

        // Keep newly neutral clusters around.
        const int cluster_lifetime = decoding_graph->d() * cluster_lifetime_factor_;
        if (current_round_ - cluster->has_been_neutral_since() < cluster_lifetime)
        {
            new_clusters.push_back(move(cluster));
            continue;
        }

        // Peel older, neutral clusters.
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
    int last_encountered_non_neutral_cluster = 0;
    last_growth_steps_ = -(rounds-1);
    for (current_round_ = 0; current_round_ < rounds; current_round_++)
    {
        last_growth_steps_ = ceil(last_growth_steps_);
        vector<shared_ptr<DecodingGraphNode>> nodes;
        for (const auto& node : marked_nodes)
        {
            if (node->id().round == current_round_ && node->marked())
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
        // Growth after adding last round belongs to the bulk growth
        if (current_round_ == rounds-1)
            break;
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
            last_growth_steps_ += 1.0/growth_rounds_;
            if (stop_early_ && Cluster::all_clusters_are_neutral(m_clusters))
            {
                break;
            }
        }
        new_error_edges = clean(decoding_graph_);
        error_edges.insert(error_edges.end(), new_error_edges.begin(), new_error_edges.end());
        if (stop_early_ && Cluster::all_clusters_are_neutral(m_clusters))
        {
            if (current_round_-last_encountered_non_neutral_cluster >= (decoding_graph_->d()-1)/2)
            {
                considered_up_to_round = current_round_;
                break;
            }
        }
        else
        {
            last_encountered_non_neutral_cluster = current_round_;
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
        if (cluster->is_neutral())
        {
            cluster->set_has_been_neutral_since(current_round_);
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

void ClAYGDecoder::merge(const vector<DecodingGraphEdge::FusionEdge>& fusion_edges)
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

        if (cluster->is_neutral())
            cluster->set_has_been_neutral_since(current_round_);

        auto cluster_to_be_removed = find(m_clusters.begin(), m_clusters.end(), other_cluster);
        if (cluster_to_be_removed != m_clusters.end())
            m_clusters.erase(cluster_to_be_removed);
    }
}
