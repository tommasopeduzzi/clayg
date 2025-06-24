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
            content << edge->id().type << "-" << edge->id().round << "-" << edge->id().id << "," << cluster_id << "\n";
        }
        cluster_id++;
    }
    std::string dir = "data/runs/" + std::to_string(run_id);
    std::filesystem::create_directories(dir);
    std::string filename = dir + "/clusters_" + decoder + "_step_" + std::to_string(step) + ".txt";
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
    std::string dir = "data/runs/" + std::to_string(run_id);
    std::filesystem::create_directories(dir);
    std::string filename = dir + "/corrections_" + decoder + ".txt";
    write_to_file(filename, content.str(), false);
}

void Logger::prepare_run_dir() const {
    std::string run_dir = "runs/" + std::to_string(run_id);
    std::filesystem::create_directories(run_dir);
    clear_files_by_pattern(run_dir, "clusters_");
    clear_files_by_pattern(run_dir, "graph.txt");
    clear_files_by_pattern(run_dir, "errors.txt");
    clear_files_by_pattern(run_dir, "corrections_");
}

void Logger::set_distance(int distance) {
    distance_ = distance;
}

int Logger::get_distance() const {
    return distance_;
}

void Logger::prepare_results_file(const std::string& decoder_name) {
    std::filesystem::create_directories(results_dir_);
    std::string filename;
    if (distance_ > 0) {
        filename = results_dir_ + "/results_d=" + std::to_string(distance_) + ".txt";
    } else {
        filename = results_dir_ + "/results.txt";
    }
    std::cout << "Writing results to " << filename << std::endl;
    write_to_file(filename, "p    \t" + decoder_name + "  \t\n", true);
}

void Logger::prepare_growth_steps_file(const std::string& decoder_name) {
    std::filesystem::create_directories(results_dir_);
    std::string filename;
    if (distance_ > 0) {
        filename = results_dir_ + "/average_operations_d=" + std::to_string(distance_) + ".txt";
    } else {
        filename = results_dir_ + "/average_operations.txt";
    }
    std::cout << "Writing average growth steps to " << filename << std::endl;
    write_to_file(filename, "p    \t" + decoder_name + "  \t\n", true);
}

void Logger::log_results(const std::string& line) {
    std::filesystem::create_directories(results_dir_);
    std::string filename;
    if (distance_ > 0) {
        filename = results_dir_ + "/results_d=" + std::to_string(distance_) + ".txt";
    } else {
        filename = results_dir_ + "/results.txt";
    }
    write_to_file(filename, line, true);
}

void Logger::log_growth_steps(const std::string& line) {
    std::filesystem::create_directories(results_dir_);
    std::string filename;
    if (distance_ > 0) {
        filename = results_dir_ + "/average_operations_d=" + std::to_string(distance_) + ".txt";
    } else {
        filename = results_dir_ + "/average_operations.txt";
    }
    write_to_file(filename, line, true);
}

void Logger::log_results_entry(double p, double value) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%g\t%.10g\n", p, value);
    log_results(std::string(buf));
}

void Logger::log_growth_steps_entry(double p, double value) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%g\t%.10g\n", p, value);
    log_growth_steps(std::string(buf));
}

void Logger::set_growth_steps(int steps) {
    growth_steps = steps;
}

int Logger::get_growth_steps() const {
    return growth_steps;
}

void Logger::increment_growth_steps() {
    growth_steps++;
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
