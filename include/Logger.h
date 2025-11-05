#pragma once
#include <fstream>
#include <string>
#include <vector>
#include <memory>

#include "DecodingGraph.h"
#include "Cluster.h"

class Logger {
public:
    Logger() = default;
    ~Logger() = default;

    // Log a message to a file (overwrites by default)
    static void write_to_file(const std::string& filename, const std::string& content, bool append = false);

    // Directory and file management
    static void create_directory(const std::string& path);
    static void remove_file(const std::string& path);
    static void copy_file(const std::string& from, const std::string& to);
    static void clear_files_by_pattern(const std::string& dir, const std::string& pattern);

    // Structured logging APIs (no filename/directory args)
    void log_clusters(const std::vector<std::shared_ptr<Cluster>>& clusters, const std::string& decoder, int step) const;
    void log_graph(const std::vector<std::shared_ptr<DecodingGraphEdge>>& edges) const;
    void log_errors(const std::vector<DecodingGraphEdge::Id>& error_ids) const;
    void log_corrections(const std::vector<DecodingGraphEdge::Id>& correction_ids, const std::string& decoder) const;
    void log_results_entry(double logical_error_rate, int runs, double p, double idling_time_constant, const std::string& decoder_name);
    void log_growth_steps(double p, const std::map<double, int>& frequencies, const std::string& decoder_name);
    void prepare_results_file(const std::string& decoder_name);
    void prepare_steps_file(const std::string& decoder_name);
    void prepare_dump_dir() const;

    // Dump flag management
    void set_dump_enabled(bool enabled);
    bool is_dump_enabled() const;

    // Dump dir management
    void set_dump_dir(const std::string& dump_dir);
    std::string get_dump_dir();

    // Run ID management
    void set_run_id(int id);
    int get_run_id() const;
    void increment_run_id(int by = 1);

    // Distance management
    void set_distance(int distance);
    int get_distance() const;

    // Progress logging
    static void log_progress(int current, int total, double p, int D, int interval_ms = 200);

    // Singleton/global instance accessor (optional)
    static Logger& instance();

    // Results path management
    void set_results_dir(const std::string& dir);
    std::string get_results_dir() const;

private:
    bool dump_enabled = false;
    int run_id = 0;
    std::string dump_dir_ = "data/runs";
    std::string results_dir_ = "data/results";
    int distance_ = -1;
};

extern Logger logger;

