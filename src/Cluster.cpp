//
// Created by tommasopeduzzi on 1/12/24.
//

#include <memory>

#include "Cluster.h"
#include "DecodingGraph.h"

using namespace std;

Cluster::Cluster(shared_ptr<DecodingGraphNode> root)
{
    m_root = root;
    m_nodes.push_back(root);
    if (root->id().type == DecodingGraphNode::VIRTUAL)
    {
        m_virtual_nodes.push_back(root);
    }
    m_marked_nodes = {};
    m_boundary = {};
    for (const auto& edge_weak_ptr : root->edges())
    {
        m_boundary.push_back({
            root,
            edge_weak_ptr.lock()->other_node(root).lock(),
            edge_weak_ptr.lock()
        });
    }
}

bool Cluster::is_neutral(const bool consider_virtual_nodes) const
{
    if (m_marked_nodes.size() % 2 == 0)
        return true;
    if (consider_virtual_nodes)
        return !m_virtual_nodes.empty();
    return false;
}

bool Cluster::all_clusters_are_neutral(vector<shared_ptr<Cluster>> clusters, bool consider_virtual_nodes)
{
    return all_of(clusters.begin(), clusters.end(),
                  [consider_virtual_nodes](const shared_ptr<Cluster>& cluster)
                  {
                      return cluster->is_neutral(consider_virtual_nodes);
                  });
}
