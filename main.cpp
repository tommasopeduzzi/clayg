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

using namespace std;

unordered_map<string, string> parse_args(int argc, char* argv[])
{
    unordered_map<string, string> args;
    vector<string> keys = {"D", "T", "p_start", "p_end", "results", "mode", "p_step", "dump"};
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
        cerr << "Usage: " << argv[0] << " D T p_start p_end decoder results mode [p_step] [dump]\n";
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

void compare(int D, int T, double p_start, double p_end, double p_step, const string& results_file, bool dump)
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
    std::regex cluster_filename_pattern("current_clusters_(uf|clayg)_(\\d+)\\.txt");

    // open results file
    cout << "Writing results to " << results_file << endl;
    ofstream results(results_file, ios::app);
    results << "p    \t";
    for (const auto& decoder : decoders)
    {
        results << decoder->decoder() << "  \t";
    }
    results << endl;

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

                auto corrections = decoder->decode(graph, dump, "");

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
            for (auto& [decoder, logical] : logicals)
            {
                if (logical != logicals.begin()->second)
                {
                    if (!dump)
                    {
                        break;
                    }

                    // make directory in runs
                    string directory_path = "data/comparisons/" + to_string(i);
                    filesystem::create_directories(directory_path);

                    graph->dump(directory_path + "/graph.txt");

                    ofstream file(directory_path + "/errors.txt");
                    for (const auto& [type, round, id] : error_edge_ids)
                    {
                        file << type << "-" << round << "-" << id << endl;
                    }
                    file.close();

                    for (auto& [decoder, corrections] : decoder_corrections)
                    {
                        filesystem::create_directories(directory_path + "/" + decoder->decoder());
                        ofstream corrections_file(directory_path + "/" + decoder->decoder() + "/corrections.txt");
                        for (const auto& [type, round, id] : corrections)
                        {
                            corrections_file << type << "-" << round << "-" << id << endl;
                        }
                        corrections_file.close();
                    }

                    for (const auto& entry : filesystem::directory_iterator("data/comparisons"))
                    {
                        if (entry.is_regular_file())
                        {
                            std::string filename = entry.path().filename().string();
                            std::smatch match;

                            // Match filename using regex pattern
                            if (!std::regex_match(filename, match, cluster_filename_pattern))
                            {
                                continue;
                            }

                            std::string current_decoder = match[1].str();
                            std::string number = match[2].str();
                            std::string new_filename = "cluster_" + number + ".txt";

                            // Create target directory: data/comparisons/<i>/<decoder>
                            std::string target_dir = "data/comparisons/" + std::to_string(i) + "/" + current_decoder;
                            filesystem::create_directories(target_dir);

                            // Copy file to target path with the new filename
                            filesystem::path target_path = filesystem::path(target_dir) / new_filename;
                            copy_file(entry.path(), target_path, filesystem::copy_options::overwrite_existing);

                            // Remove the original file
                            filesystem::remove(entry.path());
                        }
                    }
                    break;
                }
            }
            for (const auto& entry : filesystem::directory_iterator("data/comparisons"))
            {
                std::string filename = entry.path().filename().string();

                // Match filename using regex pattern
                if (std::smatch match; !std::regex_match(filename, match, cluster_filename_pattern))
                {
                    continue;
                }
                filesystem::path file_path = filesystem::path("data/comparisons") / filename;
                filesystem::remove(file_path);
            }

            if (i % (10000/(D*D)) < 2)
            {
                // delete last line in cout
                cout << "\r";
                // print progress
                cout << "p=" << p << ": " << i + 1 << " / 10000";
            }
        }

        // open the file
        ofstream file(results_file, ios::app);
        file << p << "\t";
        for (auto& [decoder, error] : errors)
        {
            double error_rate = static_cast<double>(error) / 10000;
            file << error_rate << "\t";
        }
        file << endl;

        p += p_step;
    }
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
    string results_file = args["results"];
    bool dump = string(args["dump"]) == "true";
    string mode = args["mode"];

    if (mode == "compare")
    {
        cout << results_file << endl;
        compare(D, T, p_start, p_end, p_step, results_file, dump);
        return 0;
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

    int run_id = 0;

    double p = p_start;
    while (p <= p_end)
    {
        int errors = 0;

        for (int i = 0; i < 10000; i++)
        {
            if (dump)
            {
                // make directory in runs
                system(("mkdir -p runs/" + to_string(run_id)).c_str());
                // delete old cluster files
                system(("rm -f runs/" + to_string(run_id) + "/clusters_*.txt").c_str());
            }

            graph->reset();

            error_edge_ids = generate_errors(D, T, p, dis, gen);
            /*error_edge_ids = {
                DecodingGraphEdge::Id {DecodingGraphEdge::MEASUREMENT, 5, 6},
                DecodingGraphEdge::Id {DecodingGraphEdge::MEASUREMENT, 5, 14},
                DecodingGraphEdge::Id {DecodingGraphEdge::NORMAL, 6, 10},
            };*/


            // dump errors
            if (dump)
            {
                ofstream file("runs/" + to_string(run_id) + "/errors.txt");
                for (const auto& [type, round, id] : error_edge_ids)
                {
                    file << type << "-" << round << "-" << id << endl;
                }
                file.close();
            }

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

            auto corrections = decoder->decode(graph, dump, to_string(run_id));

            logical = compute_logical(logical, logical_edge_ids, corrections);
            errors += logical;
            run_id += logical;

            // update last line of cout to show progress
            if ((i % (10000/(D*D)))< 2)
            {
                // delete last line in cout
                cout << "\r";
                // print progress
                cout << "p=" << p << ": " << i + 1 << " / 10000";
            }
        }

        // open the results file and append the results
        ofstream file(results_file, ios::app);
        double error_rate = static_cast<double>(errors) / 10000;
        file << p << "\t" << error_rate << endl;

        p += p_step;
    }
}
