#include <iostream>
#include <algorithm>
#include <fstream>
#include <random>
#include <unordered_map>

#include "DecodingGraph.h"
#include "UnionFindDecoder.h"
#include "ClAYGDecoder.h"

using namespace std;

unordered_map<string, string> parse_args(int argc, char* argv[])
{
    unordered_map<string, string> args;
    vector<string> keys = {"D", "T", "p_start", "p_end", "p_step", "dump", "mode"};
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
        cerr << "Usage: " << argv[0] << " D T p_start p_end p_step decoder [dump]\n";
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

int compare(int D, int T, double p_start, double p_end, double p_step, bool dump)
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

            for (auto& decoder : decoders)
            {
                graph->reset();
                graph->mark(error_edges);

                auto corrections = decoder->decode(graph, false, "");

                int logical = 0;
                logical = compute_logical(logical, logical_edge_ids, error_edges);
                logical = compute_logical(logical, logical_edge_ids, corrections);

                logicals[decoder] = logical;
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
                    system(("mkdir -p data/comparisons/" + to_string(i)).c_str());

                    graph->dump("data/comparisons/" + to_string(i) + "/graph.txt");

                    ofstream file("data/comparisons/" + to_string(i) + "/errors.txt");
                    for (const auto& [type, round, id] : error_edge_ids)
                    {
                        file << type << "-" << round << "-" << id << endl;
                    }
                    file.close();

                    break;
                }
            }
        }

        cout << p << "\t";
        for (auto& [decoder, error] : errors)
        {
            double error_rate = static_cast<double>(error) / 10000;
            cout << error_rate << "\t";
        }
        cout << endl;

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
    bool dump = string(args["dump"]) == "true";
    string mode = args["mode"];

    if (mode == "compare")
    {
        return compare(D, T, p_start, p_end, p_step, dump);
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
            // only bother to dump when there are actually errors.
            bool current_dump = !error_edge_ids.empty() && dump;

            if (current_dump)
            {
                // make directory in runs
                system(("mkdir -p runs/" + to_string(run_id)).c_str());
                // delete old cluster files
                system(("rm -f runs/" + to_string(run_id) + "/clusters_*.txt").c_str());
            }

            graph->reset();

            error_edge_ids = generate_errors(D, T, p, dis, gen);

            // dump errors
            if (current_dump)
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

            auto corrections = decoder->decode(graph, current_dump, to_string(run_id));

            logical = compute_logical(logical, logical_edge_ids, corrections);
            errors += logical;
            run_id += logical;
        }

        double error_rate = static_cast<double>(errors) / 10000;
        cout << p << "\t" << error_rate << endl;
        p += p_step;
    }
}
