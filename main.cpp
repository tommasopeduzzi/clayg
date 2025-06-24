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
    vector<string> keys = {"D", "T", "p_start", "p_end", "results", "mode", "p_step", "dump", "runs"};
    vector<string> modes = {"clayg", "unionfind", "compare", "none"};

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

    // Mode
    if (args.find("mode") != args.end() &&
        find(modes.begin(), modes.end(), args["mode"]) == modes.end())
    {
        cerr << "Error: Invalid mode '" << args["mode"] << "'.\n";
        exit(1);
    }

    if (args.size() != keys.size())
    {
        cerr << "Error: Invalid number of arguments.\n";
        cerr << "Usage: " << argv[0] << " D T p_start p_end decoder results mode [p_step] [dump] [runs]\n";
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

int compare(int D, int T, double p_start, double p_end, double p_step, const string& results_file, bool dump)
{
    auto uf_decoder = make_shared<UnionFindDecoder>();
    auto clayg_decoder = make_shared<ClAYGDecoder>();
    vector<shared_ptr<Decoder>> decoders = {uf_decoder, clayg_decoder};

    auto graph = DecodingGraph::rotated_surface_code(D, T);
    auto logical_edge_ids = graph->logical_edge_ids();
    vector<shared_ptr<DecodingGraphEdge>> error_edges{};
    vector<DecodingGraphEdge::Id> error_edge_ids{};

    random_device rd;
    mt19937 gen(rd());
    uniform_real_distribution<> dis(0.0, 1.0);

    // open results file
    logger.prepare_results_file("");

    double p = p_start;
    while (p <= p_end)
    {
        map<shared_ptr<Decoder>, int> errors;

        for (auto& decoder : decoders)
        {
            errors[decoder] = 0;
        }

        // for each set of errors, run each decoder. if the results differ in the final logical, save the error sets
        for (int i = 0; i < 10000; i++)
        {
            error_edge_ids = generate_errors(D, T, p, dis, gen);
            error_edges.clear();
            for (auto id : error_edge_ids)
            {
                auto edge = graph->edge(id).value();
                error_edges.push_back(edge);
            }

            map<shared_ptr<Decoder>, int> logicals;
            map<shared_ptr<Decoder>, vector<DecodingGraphEdge::Id>> decoder_corrections;

            for (auto& decoder : decoders)
            {
                graph->reset();
                graph->mark(error_edges);

                auto corrections = decoder->decode(graph);

                int logical = 0;
                logical = compute_logical(logical, logical_edge_ids, error_edges);
                logical = compute_logical(logical, logical_edge_ids, corrections);

                logicals[decoder] = logical;
                decoder_corrections[decoder] = {};
                for (auto& edge : corrections)
                {
                    decoder_corrections[decoder].push_back(edge->id());
                }
                errors[decoder] += logical;
            }

            logger.log_graph(graph->edges());
            logger.log_errors(error_edge_ids);
            for (auto& [decoder_ptr, corrections] : decoder_corrections)
            {
                logger.log_corrections(corrections, decoder_ptr->decoder());
            }

            logger.log_progress(i + 1, 10000, p, D);
        }

        // open the file
        std::string result_line = std::to_string(p) + "\t";
        for (auto& [decoder, error] : errors)
        {
            double error_rate = static_cast<double>(error) / 10000;
            result_line += std::to_string(error_rate) + "\t";
        }
        result_line += "\n";
        logger.log_results(result_line);

        p += p_step;
    }

    return 0;
}

int main(int argc, char* argv[])
{
    // Parse command line arguments in format D T p_start p_end p_step decoder
    auto args = parse_args(argc, argv);

    int D = stoi(args["D"]);
    int T = stoi(args["T"]);
    double p_start = stod(args["p_start"]);
    double p_end = stod(args["p_end"]);
    double p_step = stod(args["p_step"]);
    string results_file_path = args["results"];
    bool dump = string(args["dump"]) == "true";
    logger.set_dump_enabled(dump);
    string mode = args["mode"];
    int runs = stoi(args["runs"]);

    // Set results directory for logger
    logger.set_results_dir(args["results"]);
    // Set distance for logger (for results_d={distance}.txt and average_operations_d={distance}.txt)
    logger.set_distance(stoi(args["D"]));

    if (mode == "compare")
    {
        return compare(D, T, p_start, p_end, p_step, results_file_path, dump);
    }

    unique_ptr<Decoder> decoder = {};
    if (mode == "none")
    {
        decoder = make_unique<Decoder>();
    }
    else if (mode == "unionfind")
    {
        decoder = make_unique<UnionFindDecoder>();
    }
    else if (mode == "clayg")
    {
        decoder = make_unique<ClAYGDecoder>();
    }
    else
    {
        throw invalid_argument("Invalid decoder type: " + mode);
    }

    auto graph = DecodingGraph::rotated_surface_code(D, T);
    auto logical_edge_ids = graph->logical_edge_ids();
    vector<shared_ptr<DecodingGraphEdge>> error_edges{};
    vector<DecodingGraphEdge::Id> error_edge_ids{};

    random_device rd;
    mt19937 gen(rd());
    uniform_real_distribution<> dis(0, 1);

    double p = p_start;

    // open results file
    logger.prepare_results_file(decoder->decoder());
    logger.prepare_growth_steps_file(decoder->decoder());

    while (p <= p_end)
    {
        int errors = 0;
        logger.set_growth_steps(0);
        for (int i = 0; i < runs; i++)
        {
            logger.prepare_run_dir();

            graph->reset();

            error_edge_ids = generate_errors(D, T, p, dis, gen);

            logger.log_errors(error_edge_ids);
            logger.log_graph(graph->edges());

            error_edges.clear();
            for (auto id : error_edge_ids)
            {
                auto edge = graph->edge(id).value();
                error_edges.push_back(edge);
            }

            int logical = 0;
            logical = compute_logical(logical, logical_edge_ids, error_edges);

            // mark nodes
            graph->mark(error_edges);

            auto corrections = decoder->decode(graph);
            // convert to correction ids
            vector<DecodingGraphEdge::Id> correction_ids;
            for (auto& edge : corrections)
            {
                correction_ids.push_back(edge->id());
            }
            logger.log_corrections(correction_ids, decoder->decoder());

            logical = compute_logical(logical, logical_edge_ids, corrections);
            errors += logical;
            logger.increment_run_id(logical);

            logger.log_progress(i + 1, runs, p, D);
        }

        // open the results file and append the results
        double error_rate = static_cast<double>(errors) / runs;
        logger.log_results_entry(p, error_rate);
        double average_growth_steps = static_cast<double>(logger.get_growth_steps()) / runs;
        logger.log_growth_steps_entry(p, average_growth_steps);

        p += p_step;
    }
}
