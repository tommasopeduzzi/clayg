//
// Created by tommasopeduzzi on 1/12/24.
//

#include "Cluster.h"

Cluster::Cluster(std::shared_ptr<DecodingGraphNode> root) {
    m_root = root;
    m_nodes.push_back(root);
    if (root->id().type == DecodingGraphNode::VIRTUAL) {
        m_virtual_nodes.push_back(root);
    }
    m_marked_nodes = {};
    m_boundary = root->edges();
}

bool Cluster::is_neutral() {
    return m_marked_nodes.size() % 2 == 0 || (m_marked_nodes.size() + int(m_virtual_nodes.size() > 0)) % 2 == 0;
}

bool Cluster::all_clusters_are_neutral(std::vector<std::shared_ptr<Cluster>> clusters) {
    for (auto cluster: clusters) {
        if (!cluster->is_neutral()) {
            return false;
        }
    }
    return true;
}