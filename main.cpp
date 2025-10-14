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
    vector<string> keys = {"D", "T", "decoders", "results"};

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
    if (args.find("p_start") == args.end()) args["p_start"] = "0.005";
    if (args.find("p_end") == args.end()) args["p_end"] = "0.005";
    if (args.find("p_step") == args.end()) args["step"] = "+0.005";
    if (args.find("idling_time_constant_start") == args.end()) args["idling_time_constant_start"] = "0.0";
    if (args.find("idling_time_constant_end") == args.end()) args["idling_time_constant_end"] = "0.0";
    if (args.find("idling_time_constant_end_step") == args.end()) args["step"] = "0";
    if (args.find("dump") == args.end()) args["dump"] = "false";
    if (args.find("runs") == args.end()) args["runs"] = "10000";


    if (args.size() < 4) // D, T, decoders, results
    {
        cerr << "Error: Invalid number of arguments.\n";
        cerr << "Usage: " << argv[0] << "D T decoders results [p_start] [p_end] [p_step] [idling_time_constant_start] "
             << "[idling_time_constant_end] [idling_time_constant_step] [dump] [runs]\n";
        exit(1);
    }

    for (auto step_key : {"p_step", "idling_time_constant_step"})
    {
        if (string("*/+-#").find(args[step_key][0]) == string::npos)
        {
            cerr << "Error: Invalid " << step_key << ". Must be a floating point number prefixed by +, -, *, or /.\n";
            exit(1);
        }
        try
        {
            stod(args[step_key].substr(1));
        }
        catch (const invalid_argument& e)
        {
            cerr << "Error: Invalid argument " << step_key << ". Must be a floating point number prefixed by +, -, *, or /.\n";
            exit(1);
        }
    }

    for (auto float_key : {"p_start", "p_end", "idling_time_constant_start", "idling_time_constant_end"})
    {
        try
        {
            stod(args[float_key]);
        }
        catch (const invalid_argument& e)
        {
            cerr << "Error: Invalid argument " << float_key << ". Must be a floating point number.\n";
            exit(1);
        }
    }

    for (auto int_key : {"D", "T", "runs"})
    {
        try
        {
            stoi(args[int_key]);
        }
        catch (const invalid_argument& e)
        {
            cerr << "Error: Invalid argument " << int_key << ". Must be an integer.\n";
            exit(1);
        }
        if (stoi(args[int_key]) <= 0)
        {
            cerr << "Error: Invalid argument " << int_key << ". Must be a positive integer.\n";
            exit(1);
        }
    }

    for (auto bool_key : {"dump"})
    {
        if (args[bool_key] != "true" && args[bool_key] != "false")
        {
            cerr << "Error: Invalid argument " << bool_key << ". Must be true or false.\n";
            exit(1);
        }
    }

    return args;
}

vector<DecodingGraphEdge::Id> parse_errors(string errors)
{
    vector<DecodingGraphEdge::Id> error_edge_ids;
    regex re("(\\d+)-(\\d+)-(\\d+)");
    smatch match;
    istringstream stream(errors);
    while (getline(stream, errors))
    {
        if (regex_search(errors, match, re))
        {
            DecodingGraphEdge::Id id;
            id.type = static_cast<DecodingGraphEdge::Type>(stoi(match[1].str()));
            id.round = stoi(match[2].str());
            id.id = stoi(match[3].str());
            error_edge_ids.push_back(id);
        }
        // check if line is not only whitespace
        else if (!regex_match(errors, regex("^\\s*$")))
        {
            cerr << "Error: Invalid error format: " << errors << endl;
            exit(1);
        }
    }
    return error_edge_ids;
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
                    const DecodingResult& corrections)
{
    const auto considered_rounds = corrections.considered_up_to_round;
    const auto error_edges = corrections.corrections;
    for (const auto& error : error_edges)
    {
        // Check if edge is relevant to final measurement
        if (error->id().type == DecodingGraphEdge::Type::MEASUREMENT)
            continue;

        if (error->id().round > considered_rounds)
            continue;

        if (logical_edge_ids.find(error->id().id) == logical_edge_ids.end())
            continue;

        logical += 1;
    }
    return logical % 2;
}

void increment_by_step(double& variable, pair<char, double> step)
{
    switch (step.first)
    {
    case '+':
        variable += step.second;
        break;
    case '/':
        variable /= step.second;
        break;
    case '-':
        variable -= step.second;
        break;
    case '*':
        variable *= step.second;
        break;

    case '#': // harmonic scaling (linear in 1/x)
        {
            double inv = 1.0 / variable;
            inv += step.second;
            variable = 1.0 / inv;
        }
        break;
    default:
        throw std::invalid_argument("Unknown step type");
    }
}

bool increment_end_condition(double const current_variable, double start, double end)
{
    if (start < end)
        return current_variable > end;
    if (start > end)
        return current_variable < end;
    return true;
}

int main(int argc, char* argv[])
{
    // Parse command line arguments in format D T p_start p_end decoders results [step] [dump] [runs]
    auto args = parse_args(argc, argv);

    int D = stoi(args["D"]);
    int T = stoi(args["T"]);
    double p_start = stod(args["p_start"]);
    double p_end = stod(args["p_end"]);
    pair<char, double> p_step = {args["p_step"][0], stod(args["p_step"].substr(1))};
    double idling_time_constant_start = stod(args["idling_time_constant_start"]);
    double idling_time_constant_end = stod(args["idling_time_constant_end"]);
    pair<char, double> idling_time_constant_step = {args["idling_time_constant_step"][0], stod(args["idling_time_constant_step"].substr(1))};
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
        } else if (name == "single_layer_clayg" || name == "sl_clayg") {
            decoders.push_back(make_shared<SingleLayerClAYGDecoder>());
        } else if (name == "clayg_stop_early") {
            auto decoder = make_shared<ClAYGDecoder>();
            decoder->set_stop_early(true);
            decoder->set_decoder_name("clayg_stop_early");
            decoders.push_back(decoder);
        } else if (name == "clayg_third_growth") {
            auto decoder = make_shared<ClAYGDecoder>();
            decoder->set_growth_policy([] (const DecodingGraphNode::Id start, const DecodingGraphNode::Id end)
            {
                if (start.round == end.round)
                    return 0.34;
                if (start.round > end.round)
                    return 1.0;
                return 0.5;
            });
            decoder->set_decoder_name("clayg_third_growth");
            decoders.push_back(decoder);
        }
        else if (name == "clayg_faster_backwards_growth") {
            auto decoder = make_shared<ClAYGDecoder>();
            decoder->set_growth_policy([] (const DecodingGraphNode::Id start, const DecodingGraphNode::Id end)
            {
                if (start.round > end.round)
                    return 1.0;
                return 0.5;
            });
            decoder->set_decoder_name("clayg_faster_backwards_growth");
            decoders.push_back(decoder);
        } else if (name == "sl_clayg_two_growth_rounds") {
            auto decoder = make_shared<SingleLayerClAYGDecoder>();
            decoder->set_growth_rounds(2);
            decoder->set_decoder_name("sl_clayg_two_growth_rounds");
            decoders.push_back(decoder);
        } else if (name == "sl_clayg_stop_early") {
            auto decoder = make_shared<SingleLayerClAYGDecoder>();
            decoder->set_decoder_name("sl_clayg_stop_early");
            decoder->set_stop_early(true);
            decoders.push_back(decoder);
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
        logger.prepare_results_file(decoder->decoder_name());
        logger.prepare_steps_file(decoder->decoder_name());
    }

    vector<pair<double, map<string, double>>> results;
    static auto last_three_runs_corrected = [&] ()
    {
        return std::size(results) >= 3 &&
            std::all_of(results.end() - 3, results.end(), [&](const auto& run) {
                const auto& results_per_decoder = run.second;
                return std::all_of(decoders.begin(), decoders.end(), [&](const auto& decoder) {
                    return results_per_decoder.at(decoder->decoder_name()) == 0;
                });
            });
    };

    do
    {
        // decoder name -> idling time constant -> total logical errors
        map<string, map<double, int>> errors;
        map<string, map<double, int>> growth_steps;
        for (const auto& decoder : decoders) {
            errors[decoder->decoder_name()] = {};
            // always compute for idling time constant 0.0 for last three corrected runs condition
            errors[decoder->decoder_name()][0.0] = 0;
            auto idling_time_constant = idling_time_constant_start;
            while (!increment_end_condition(idling_time_constant, idling_time_constant_start, idling_time_constant_end))
            {
                errors[decoder->decoder_name()][idling_time_constant] = 0;
                increment_by_step(idling_time_constant, idling_time_constant_step);
            }
            growth_steps[decoder->decoder_name()] = {};
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
                graph->reset();
                graph->mark(error_edges);
                auto decoding_results = decoder->decode(graph);
                vector<DecodingGraphEdge::Id> correction_ids;
                for (auto& edge : decoding_results.corrections) {
                    correction_ids.push_back(edge->id());
                }
                logger.log_corrections(correction_ids, decoder->decoder_name());
                int logical = compute_logical(0, logical_edge_ids, {
                    error_edges,
                    decoding_results.considered_up_to_round
                });
                int logical_after_decoding = compute_logical(logical, logical_edge_ids, decoding_results);
                if (logical_after_decoding != 0) {
                    uncorrected = true;
                }

                int num_growth_steps = decoder->get_last_growth_steps();
                growth_steps[decoder->decoder_name()][num_growth_steps] += 1;

                errors[decoder->decoder_name()][0.0] += logical_after_decoding;

                double idling_time_constant = idling_time_constant_start;
                while (!increment_end_condition(idling_time_constant, idling_time_constant_start, idling_time_constant_end))
                {
                    if (idling_time_constant == 0.0) // already computed above
                    {
                        increment_by_step(idling_time_constant, idling_time_constant_step);
                        continue;
                    }
                    double p_idling = 0.5 * (1-exp(-(num_growth_steps/idling_time_constant)));
                    auto idling_error_edge_ids = generate_errors(D, 1, p_idling, dis, gen);
                    auto idling_error_edges = vector<shared_ptr<DecodingGraphEdge>>{};
                    for (auto id : idling_error_edge_ids)
                    {
                        auto edge = graph->edge(id).value();
                        idling_error_edges.push_back(edge);
                    }
                    int logical_after_idling = compute_logical(logical_after_decoding, logical_edge_ids, {
                        idling_error_edges,
                        decoding_results.considered_up_to_round
                    });
                    errors[decoder->decoder_name()][idling_time_constant] += logical_after_idling;
                    increment_by_step(idling_time_constant, idling_time_constant_step);
                }
            }
            if (uncorrected)
            {
                logger.increment_run_id();
            }
            Logger::log_progress(i + 1, runs, p, D);
        }
        // Log results and average growth steps for each decoder
        results.push_back({p, {}});
        for (const auto& decoder : decoders) {
            for (const auto& [idling_time_constant, logical_errors] : errors[decoder->decoder_name()]) {
                double logical_error_rate = static_cast<double>(logical_errors) / runs;
                logger.log_results_entry(logical_error_rate, runs, p,  idling_time_constant, decoder->decoder_name());
                if (idling_time_constant == 0.0)
                    results.back().second[decoder->decoder_name()] = logical_error_rate;
            }
            logger.log_growth_steps(p, growth_steps[decoder->decoder_name()], decoder->decoder_name());
        }
        increment_by_step(p, p_step);
    } while (!increment_end_condition(p, p_start, p_end) || last_three_runs_corrected());
    return 0;
}
