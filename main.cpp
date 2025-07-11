#include <iostream>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <regex>
#include <unordered_map>

#include "DecodingGraph.h"
#include "UnionFindDecoder.h"
#include "ClAYGDecoder.h"
#include "Logger.h"

using namespace std;

unordered_map<string, string> parse_args(int argc, char* argv[])
{
    unordered_map<string, string> args;
    vector<string> keys = {"D", "T", "p_start", "p_end", "decoders", "results", "p_step", "dump", "runs"};

    for (int i = 1; i < argc; ++i)
    {
        string arg = argv[i];
        size_t eq_pos = arg.find('=');

        if (eq_pos != string::npos)
        {
            string key = arg.substr(0, eq_pos);
            string value = arg.substr(eq_pos + 1);
            args[key] = value;
        }
        else if (i - 1 < keys.size())
        {
            args[keys[i - 1]] = arg;
        }
    }

    // Set defaults
    if (args.find("p_step") == args.end()) args["p_step"] = "0.005";
    if (args.find("dump") == args.end()) args["dump"] = "false";
    if (args.find("runs") == args.end()) args["runs"] = "10000";

    if (args.size() < 6) // D, T, p_start, p_end, decoders, results
    {
        cerr << "Error: Invalid number of arguments.\n";
        cerr << "Usage: " << argv[0] << " D T p_start p_end decoders results [p_step] [dump] [runs]\n";
        exit(1);
    }

    if (args["p_step"][0] != '*' && args["p_step"][0] != '+')
    {
        cerr << "Error: Invalid p_step.\n";
        cerr << "Usage: p_step=*{factor} or p_step=+{step}\n";
        exit(1);
    }

    return args;
}

vector<DecodingGraphEdge::Id> generate_errors(int D, int T, double p, uniform_real_distribution<double>& dis,
                                              mt19937& gen)
{
    vector<DecodingGraphEdge::Id> error_edge_ids;
    for (int t = 0; t < T; t++)
    {
        for (int edge = 0; edge < D * D; edge++)
        {
            if (dis(gen) <= p)
            {
                auto id = DecodingGraphEdge::Id{DecodingGraphEdge::Type::NORMAL, t, edge};
                error_edge_ids.push_back(id);
            }
        }
        if (t == T - 1) continue;
        for (int edge = 0; edge < (D - 1) * (int(D / 2) + 1); edge++)
        {
            if (dis(gen) <= p)
            {
                auto id = DecodingGraphEdge::Id{DecodingGraphEdge::Type::MEASUREMENT, t, edge};
                error_edge_ids.push_back(id);
            }
        }
    }
    return error_edge_ids;
}

int compute_logical(int logical, const set<int>& logical_edge_ids,
                    const vector<shared_ptr<DecodingGraphEdge>>& error_edges)
{
    for (const auto& error : error_edges)
    {
        // Check if edge is relevant to final measurement
        if (error->id().type == DecodingGraphEdge::Type::MEASUREMENT)
            continue;

        if (logical_edge_ids.find(error->id().id) == logical_edge_ids.end())
            continue;

        logical += 1;
    }
    return logical % 2;
}

int main(int argc, char* argv[])
{
    // Parse command line arguments in format D T p_start p_end decoders results [p_step] [dump] [runs]
    auto args = parse_args(argc, argv);

    int D = stoi(args["D"]);
    int T = stoi(args["T"]);
    double p_start = stod(args["p_start"]);
    double p_end = stod(args["p_end"]);
    pair<char, double> p_step = {args["p_step"][0], stod(args["p_step"].substr(1))};
    string results_file_path = args["results"];
    bool dump = string(args["dump"]) == "true";
    logger.set_dump_enabled(dump);
    int runs = stoi(args["runs"]);

    // Parse decoders argument (comma-separated)
    vector<string> decoder_names;
    string decoders_arg = args["decoders"];
    size_t start = 0, end = 0;
    while ((end = decoders_arg.find(',', start)) != string::npos) {
        decoder_names.push_back(decoders_arg.substr(start, end - start));
        start = end + 1;
    }
    decoder_names.push_back(decoders_arg.substr(start));

    // Instantiate decoders
    vector<shared_ptr<Decoder>> decoders;
    for (const auto& name : decoder_names) {
        if (name == "uf" || name == "unionfind") {
            decoders.push_back(make_shared<UnionFindDecoder>());
        } else if (name == "clayg") {
            decoders.push_back(make_shared<ClAYGDecoder>());
        } else {
            cerr << "Unknown decoder: " << name << endl;
            exit(1);
        }
    }

    // Set results directory for logger
    logger.set_results_dir(args["results"]);
    // Set distance for logger (for results_d={distance}.txt and average_operations_d={distance}.txt)
    logger.set_distance(stoi(args["D"]));

    auto graph = DecodingGraph::rotated_surface_code(D, T);
    auto logical_edge_ids = graph->logical_edge_ids();
    vector<shared_ptr<DecodingGraphEdge>> error_edges{};
    vector<DecodingGraphEdge::Id> error_edge_ids{};

    random_device rd;
    mt19937 gen(rd());
    uniform_real_distribution<> dis(0, 1);

    double p = p_start;

    // Prepare results and growth steps files for each decoder
    for (const auto& decoder : decoders) {
        logger.prepare_results_file(decoder->decoder());
        logger.prepare_growth_steps_file(decoder->decoder());
    }

    while (p <= p_end)
    {
        map<string, int> errors;
        map<string, double> growth_steps;
        for (const auto& decoder : decoders) {
            errors[decoder->decoder()] = 0;
            growth_steps[decoder->decoder()] = 0.0;
        }

        for (int i = 0; i < runs; i++)
        {
            logger.prepare_run_dir();
            error_edge_ids = generate_errors(D, T, p, dis, gen);
            logger.log_errors(error_edge_ids);
            logger.log_graph(graph->edges());
            error_edges.clear();
            for (auto id : error_edge_ids)
            {
                auto edge = graph->edge(id).value();
                error_edges.push_back(edge);
            }
            bool uncorrected = false;
            for (const auto& decoder : decoders) {
                int logical = 0;
                logical = compute_logical(logical, logical_edge_ids, error_edges);
                graph->reset();
                graph->mark(error_edges);
                auto corrections = decoder->decode(graph);
                vector<DecodingGraphEdge::Id> correction_ids;
                for (auto& edge : corrections) {
                    correction_ids.push_back(edge->id());
                }
                logger.log_corrections(correction_ids, decoder->decoder());
                int logical_result = compute_logical(logical, logical_edge_ids, corrections);
                errors[decoder->decoder()] += logical_result;
                growth_steps[decoder->decoder()] += decoder->get_last_growth_steps();
                if (logical_result != logical) {
                    uncorrected = true;
                }
            }
            if (uncorrected)
            {
                logger.increment_run_id();
            }
            Logger::log_progress(i + 1, runs, p, D);
        }
        // Log results and average growth steps for each decoder
        for (const auto& decoder : decoders) {
            double error_rate = static_cast<double>(errors[decoder->decoder()]) / runs;
            logger.log_results_entry(p, error_rate, decoder->decoder());
            double avg_growth = growth_steps[decoder->decoder()] / runs;
            logger.log_growth_steps_entry(p, avg_growth, decoder->decoder());
        }
        switch (p_step.first)
        {
        case '+':
            p += p_step.second;
            break;
        case '*':
        default:
            p *= p_step.second;
            break;
        }
    }
    return 0;
}
