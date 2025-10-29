#include <iostream>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <regex>
#include <unordered_map>
#include <unordered_set>

#include "DecodingGraph.h"
#include "UnionFindDecoder.h"
#include "ClAYGDecoder.h"
#include "Logger.h"

using namespace std;

struct ArgSpec {
    string name;
    string default_value;
    function<void(const string&)> validator;
};

struct DecoderConfig {
    string name;
    unordered_map<string, string> args;
};

vector<DecoderConfig> parse_decoder_list(const string& input) {
    vector<DecoderConfig> decoders;
    size_t pos = 0;
    while (pos < input.size()) {
        size_t start = pos;
        size_t paren = input.find('(', start);
        size_t comma = input.find(',', start);
        DecoderConfig cfg;

        if (paren != string::npos && (comma == string::npos || paren < comma)) {
            cfg.name = input.substr(start, paren - start);
            size_t close = input.find(')', paren);
            if (close == string::npos)
                throw invalid_argument("Missing ')' in decoder list for: " + cfg.name);

            string inside = input.substr(paren + 1, close - paren - 1);
            stringstream ss(inside);
            string kv;
            while (getline(ss, kv, ',')) {
                if (kv.empty()) continue;
                size_t eq = kv.find('=');
                if (eq == string::npos)
                    throw invalid_argument("Invalid decoder arg in " + cfg.name + ": " + kv);
                cfg.args[kv.substr(0, eq)] = kv.substr(eq + 1);
            }
            pos = close + 1;
        } else {
            size_t end = (comma == string::npos) ? input.size() : comma;
            cfg.name = input.substr(start, end - start);
            pos = end;
        }

        while (pos < input.size() && (input[pos] == ',' || isspace(input[pos])))
            ++pos;

        if (!cfg.name.empty())
            decoders.push_back(cfg);
    }
    return decoders;
}

unordered_map<string, string> parse_args(int argc, char* argv[]) {
    vector<ArgSpec> specs = {
        {"D", "", [](const string& v){ stoi(v); }},
        {"T", "", [](const string& v){ stoi(v); }},
        {"decoders", "", [](const string& v){
            if (v.empty()) throw invalid_argument("empty decoders");
            parse_decoder_list(v);
        }},
        {"results", "", [](const string& v){ if (v.empty()) throw invalid_argument("empty results path"); }},
        {"p_start", "0.005", [](const string& v){ stod(v); }},
        {"p_end", "0.005", [](const string& v){ stod(v); }},
        {"p_step", "+0.005", [](const string& v){
            if (v.size() < 2 || string("+-*/").find(v[0]) == string::npos)
                throw invalid_argument("must start with +, -, *, or /");
            stod(v.substr(1));
        }},
        {"idling_time_constant_start", "0.0", [](const string& v){ stod(v); }},
        {"idling_time_constant_end", "0.0", [](const string& v){ stod(v); }},
        {"idling_time_constant_step", "+0.0", [](const string& v){
            if (v.size() < 2 || string("+-*/").find(v[0]) == string::npos)
                throw invalid_argument("must start with +, -, *, or /");
            stod(v.substr(1));
        }},
        {"dump", "false", [](const string& v){
            if (v != "true" && v != "false")
                throw invalid_argument("must be true or false");
        }},
        {"runs", "10000", [](const string& v){ stoi(v); }},
    };

    unordered_set<string> known_keys;
    for (auto& s : specs) known_keys.insert(s.name);

    unordered_map<string, string> args;
    vector<string> positional = {"D", "T", "decoders", "results"};
    int pos_i = 0;

    for (int i = 1; i < argc; ++i) {
        string token = argv[i];

        // Option-style argument
        if (token.rfind("--", 0) == 0) {
            string key = token.substr(2);
            if (!known_keys.count(key)) {
                cerr << "Unknown option: " << key << endl;
                exit(1);
            }

            if (i + 1 >= argc) {
                cerr << "Missing value for option: " << key << endl;
                exit(1);
            }

            string value = argv[++i];
            args[key] = value;
        }
        // Positional arguments (D, T, decoders, results)
        else if (pos_i < (int)positional.size()) {
            args[positional[pos_i++]] = token;
        } else {
            cerr << "Unexpected argument: " << token << endl;
            exit(1);
        }
    }

    // Apply defaults & validate
    for (const auto& spec : specs) {
        if (args.find(spec.name) == args.end())
            args[spec.name] = spec.default_value;

        try {
            spec.validator(args[spec.name]);
        } catch (const invalid_argument& e) {
            cerr << "Invalid argument for " << spec.name << ": " << args[spec.name]
                 << "\nReason: " << e.what() << endl;
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
    pair p_step = {args["p_step"][0], stod(args["p_step"].substr(1))};
    double idling_time_constant_start = stod(args["idling_time_constant_start"]);
    double idling_time_constant_end = stod(args["idling_time_constant_end"]);
    pair idling_time_constant_step = {args["idling_time_constant_step"][0], stod(args["idling_time_constant_step"].substr(1))};
    string results_file_path = args["results"];
    bool dump = string(args["dump"]) == "true";
    logger.set_dump_enabled(dump);
    int runs = stoi(args["runs"]);

    // Parse decoders argument (comma-separated)
    vector<string> decoder_names;
    string decoders_arg = args["decoders"];
    auto parsed_decoders = parse_decoder_list(decoders_arg);

    // Instantiate decoders
    vector<shared_ptr<Decoder>> decoders;
    for (auto& [decoder_name, decoder_args] : parsed_decoders) {
        if (decoder_name == "uf" || decoder_name == "unionfind") {
            decoders.push_back(make_shared<UnionFindDecoder>());
        } else if (decoder_name == "clayg") {
            decoders.push_back(make_shared<ClAYGDecoder>(decoder_args));
        } else if (decoder_name == "single_layer_clayg" || decoder_name == "sl_clayg") {
            decoders.push_back(make_shared<SingleLayerClAYGDecoder>(decoder_args));
        } else {
            cerr << "Unknown decoder: " << decoder_name << endl;
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
