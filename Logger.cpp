#include "Logger.h"
#include "DecodingGraph.h"
#include "Cluster.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <chrono>

Logger logger;

void Logger::write_to_file(const std::string& filename, const std::string& content, bool append) {
    std::ofstream file;
    if (append)
        file.open(filename, std::ios::app);
    else
        file.open(filename);
    if (!file) return;
    file << content;
    file.close();
}

void Logger::create_directory(const std::string& path) {
    std::filesystem::create_directories(path);
}

void Logger::remove_file(const std::string& path) {
    std::filesystem::remove(path);
}

void Logger::copy_file(const std::string& from, const std::string& to) {
    std::filesystem::copy(from, to, std::filesystem::copy_options::overwrite_existing);
}

void Logger::clear_files_by_pattern(const std::string& dir, const std::string& pattern) {
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().filename().string().find(pattern) != std::string::npos) {
            std::filesystem::remove(entry.path());
        }
    }
}

void Logger::set_dump_enabled(bool enabled) {
    dump_enabled = enabled;
}

bool Logger::is_dump_enabled() const {
    return dump_enabled;
}

void Logger::set_run_id(int id) {
    run_id = id;
}

int Logger::get_run_id() const {
    return run_id;
}

void Logger::increment_run_id(int by) {
    run_id += by;
}

void Logger::log_clusters(const std::vector<std::shared_ptr<Cluster>>& clusters, const std::string& decoder, int step) const {
    if (!dump_enabled) return;
    std::ostringstream content;
    int cluster_id = 0;
    for (const auto& cluster : clusters) {
        for (const auto& edge : cluster->edges()) {
            content << edge->id().type << "-" << edge->id().round << "-" << edge->id().id;
            auto tree_node = edge->nodes().first.lock();
            content << "," << tree_node->id().type << "-" << tree_node->id().round << "-" << tree_node->id().id;
            content << "," << "1.0" << "," << cluster_id << "\n";
        }
        for (const auto& boundary : cluster->boundary()) {
            content << boundary.edge->id().type << "-" << boundary.edge->id().round << "-" << boundary.edge->id().id;
            // log tree node
            content << "," << boundary.tree_node->id().type << "-" << boundary.tree_node->id().round << "-" << boundary.tree_node->id().id;
            // log edge growth
            content << "," << boundary.edge->growth() << "," << cluster_id << "\n";
        }
        cluster_id++;
    }
    std::string dir = "data/runs/" + std::to_string(run_id) + "/" + decoder;
    std::filesystem::create_directories(dir);
    std::string filename = dir + "/clusters_step_" + std::to_string(step) + ".txt";
    write_to_file(filename, content.str(), false);
}

void Logger::log_graph(const std::vector<std::shared_ptr<DecodingGraphEdge>>& edges) const {
    if (!dump_enabled) return;
    std::ostringstream content;
    for (const auto& edge : edges) {
        auto [node1, node2] = edge->nodes();
        auto n1 = node1.lock();
        auto n2 = node2.lock();
        if (n1 && n2) {
            content << n1->id().type << "-" << n1->id().round << "-" << n1->id().id << ","
                    << n2->id().type << "-" << n2->id().round << "-" << n2->id().id << ","
                    << edge->id().type << "-" << edge->id().round << "-" << edge->id().id << "\n";
        }
    }
    std::string dir = "data/runs/" + std::to_string(run_id);
    std::filesystem::create_directories(dir);
    std::string filename = dir + "/graph.txt";
    write_to_file(filename, content.str(), false);
}

void Logger::log_errors(const std::vector<DecodingGraphEdge::Id>& error_ids) const {
    if (!dump_enabled) return;
    std::ostringstream content;
    for (const auto& id : error_ids) {
        content << id.type << "-" << id.round << "-" << id.id << "\n";
    }
    std::string dir = "data/runs/" + std::to_string(run_id);
    std::filesystem::create_directories(dir);
    std::string filename = dir + "/errors.txt";
    write_to_file(filename, content.str(), false);
}

void Logger::log_corrections(const std::vector<DecodingGraphEdge::Id>& correction_ids, const std::string& decoder) const {
    if (!dump_enabled) return;
    std::ostringstream content;
    for (const auto& id : correction_ids) {
        content << id.type << "-" << id.round << "-" << id.id << "\n";
    }
    std::string dir = "data/runs/" + std::to_string(run_id) + "/" + decoder;
    std::filesystem::create_directories(dir);
    std::string filename = dir + "/corrections.txt";
    write_to_file(filename, content.str(), false);
}

void Logger::prepare_results_file(const std::string& decoder_name) {
    std::filesystem::create_directories(results_dir_ + "/steps");
    std::string filename;
    if (distance_ > 0) {
        filename = results_dir_ + "/results/" + decoder_name + "_d=" + std::to_string(distance_) + ".txt";
    } else {
        filename = results_dir_ + "/results/" + decoder_name + ".txt";
    }
}

void Logger::prepare_steps_file(const std::string& decoder_name) {
    std::filesystem::create_directories(results_dir_ + "/results");
}


void Logger::prepare_run_dir() const {
    if (!dump_enabled) return;
    const std::string run_dir = "data/runs/" + std::to_string(run_id);
    std::filesystem::remove_all(run_dir);
    std::filesystem::create_directories(run_dir);
}

void Logger::set_distance(int distance) {
    distance_ = distance;
}

int Logger::get_distance() const {
    return distance_;
}

void Logger::log_results_entry(double logical_error_rate, int runs, double p,  double idling_time_constant, const std::string& decoder_name) {
    std::filesystem::create_directories(results_dir_);
    std::string filename = results_dir_ + "/results/"+ decoder_name + "_";
    if (distance_ > 0) {
        filename += "d=" + std::to_string(distance_) + "_";
    }
    if (idling_time_constant > 0.0)
    {
        filename += "idlingtimeconstant=" + std::to_string(idling_time_constant) + "_";
    }
    if (filename.back() == '_') {
        filename.pop_back(); // remove trailing underscore
    }
    filename += ".txt";
    std::ostringstream line;
    line << p << "\t" << logical_error_rate << "\t" << runs << "\n";
    write_to_file(filename, line.str(), true);
}

void Logger::log_growth_steps(double p, const std::map<double, int>& frequencies, const std::string& decoder_name) {
    std::filesystem::create_directories(results_dir_);
    std::string filename;
    if (distance_ > 0) {
        filename = results_dir_ + "/steps/" + decoder_name + "_d=" +
            std::to_string(distance_) + "_p=" + std::to_string(p) +  ".txt";
    } else {
        filename = results_dir_ + "/steps/" + decoder_name + ".txt";
    }
    for (const auto& [steps, count] : frequencies) {
        std::ostringstream line;
        line << steps << "\t" << count << "\n";
        write_to_file(filename, line.str(), true);
    }
}

void Logger::log_progress(int current, int total, double p, int D, int interval_ms) {
    static auto last = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count() > interval_ms) {
        last = now;
        std::cout << "\rp=" << p << ": " << current << " / " << total << std::flush;
    }
}

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::set_results_dir(const std::string& dir) {
    results_dir_ = dir;
}
