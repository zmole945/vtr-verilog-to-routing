#include <ctime>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include "assert.hpp"
#include "sta_util.hpp"


using std::cout;
using std::endl;
void identify_constant_gen_fanout_helper(const TimingGraph& tg, const NodeId node_id, std::set<NodeId>& const_gen_fanout_nodes);

float time_sec(struct timespec start, struct timespec end) {
    float time = end.tv_sec - start.tv_sec;

    time += (end.tv_nsec - start.tv_nsec) * 1e-9;
    return time;
}

void print_histogram(const std::vector<float>& values, int nbuckets) {
    nbuckets = std::min(values.size(), (size_t) nbuckets);
    int values_per_bucket = ceil((float) values.size() / nbuckets);

    std::vector<float> buckets(nbuckets);

    //Sum up each bucket
    for(size_t i = 0; i < values.size(); i++) {
        int ibucket = i / values_per_bucket;

        buckets[ibucket] += values[i];
    }

    //Normalize to get average value
    for(int i = 0; i < nbuckets; i++) {
        buckets[i] /= values_per_bucket;
    }

    float max_bucket_val = *std::max_element(buckets.begin(), buckets.end());

    //Print the histogram
    std::ios_base::fmtflags saved_flags = cout.flags();
    std::streamsize prec = cout.precision();
    std::streamsize width = cout.width();

    std::streamsize int_width = ceil(log10(values.size()));
    std::streamsize float_prec = 1;

    int histo_char_width = 60;

    //cout << "\t" << endl;
    for(int i = 0; i < nbuckets; i++) {
        cout << std::setw(int_width) << i*values_per_bucket << ":" << std::setw(int_width) << (i+1)*values_per_bucket - 1;
        cout << " " <<  std::scientific << std::setprecision(float_prec) << buckets[i];
        cout << " ";

        for(int j = 0; j < histo_char_width*(buckets[i]/max_bucket_val); j++) {
            cout << "*";
        }
        cout << endl;
    }

    cout.flags(saved_flags);
    cout.precision(prec);
    cout.width(width);
}

float relative_error(float A, float B) {
    if (A == B) {
        return 0.;
    }

    if (fabs(B) > fabs(A)) {
        return fabs((A - B) / B);
    } else {
        return fabs((A - B) / A);
    }
}

void print_level_histogram(const TimingGraph& tg, int nbuckets) {
    cout << "Levels Width Histogram" << endl;

    std::vector<float> level_widths;
    for(int i = 0; i < tg.num_levels(); i++) {
        level_widths.push_back(tg.level(i).size());
    }
    print_histogram(level_widths, nbuckets);
}

void print_node_fanin_histogram(const TimingGraph& tg, int nbuckets) {
    cout << "Node Fan-in Histogram" << endl;

    std::vector<float> fanin;
    for(NodeId i = 0; i < tg.num_nodes(); i++) {
        fanin.push_back(tg.num_node_in_edges(i));
    }

    std::sort(fanin.begin(), fanin.end(), std::greater<float>());
    print_histogram(fanin, nbuckets);
}

void print_node_fanout_histogram(const TimingGraph& tg, int nbuckets) {
    cout << "Node Fan-out Histogram" << endl;

    std::vector<float> fanout;
    for(NodeId i = 0; i < tg.num_nodes(); i++) {
        fanout.push_back(tg.num_node_out_edges(i));
    }

    std::sort(fanout.begin(), fanout.end(), std::greater<float>());
    print_histogram(fanout, nbuckets);
}


void print_timing_graph(const TimingGraph& tg) {
    for(NodeId node_id = 0; node_id < tg.num_nodes(); node_id++) {
        cout << "Node: " << node_id;
        cout << " Type: " << tg.node_type(node_id);
        cout << " Out Edges: " << tg.num_node_out_edges(node_id);
        cout << " is_clk_src: " << tg.node_is_clock_source(node_id);
        cout << endl;
        for(int out_edge_idx = 0; out_edge_idx < tg.num_node_out_edges(node_id); out_edge_idx++) {
            EdgeId edge_id = tg.node_out_edge(node_id, out_edge_idx);
            ASSERT(tg.edge_src_node(edge_id) == node_id);

            NodeId sink_node_id = tg.edge_sink_node(edge_id);

            cout << "\tEdge src node: " << node_id << " sink node: " << sink_node_id << " Delay: " << tg.edge_delay(edge_id).value() << endl;
        }
    }
}

void print_levelization(const TimingGraph& tg) {
    cout << "Num Levels: " << tg.num_levels() << endl;
    for(int ilevel = 0; ilevel < tg.num_levels(); ilevel++) {
        const auto& level = tg.level(ilevel);
        cout << "Level " << ilevel << ": " << level.size() << " nodes" << endl;
        cout << "\t";
        for(auto node_id : level) {
            cout << node_id << " ";
        }
        cout << endl;
    }
}


void print_timing_tags_histogram(const TimingGraph& tg, SerialTimingAnalyzer& analyzer, int nbuckets) {
    const int int_width = 8;
    const int flt_width = 2;

    auto totaler = [](int total, const std::map<int,int>::value_type& kv) {
        return total + kv.second;
    };

    cout << "Node Data Tag Count Histogram:" << endl;
    std::map<int,int> data_tag_cnts;
    for(NodeId i = 0; i < tg.num_nodes(); i++) {
        data_tag_cnts[analyzer.data_tags(i).num_tags()]++;
    }

    int total_data_tags = std::accumulate(data_tag_cnts.begin(), data_tag_cnts.end(), 0, totaler);
    for(const auto& kv : data_tag_cnts) {
        cout << "\t" << kv.first << " Tags: " << std::setw(int_width) << kv.second << " (" << std::setw(flt_width) << std::fixed << (float) kv.second / total_data_tags << ")" << endl;
    }

    cout << "Node clock Tag Count Histogram:" << endl;
    std::map<int,int> clock_tag_cnts;
    for(NodeId i = 0; i < tg.num_nodes(); i++) {
        clock_tag_cnts[analyzer.clock_tags(i).num_tags()]++;
    }

    int total_clock_tags = std::accumulate(clock_tag_cnts.begin(), clock_tag_cnts.end(), 0, totaler);
    for(const auto& kv : clock_tag_cnts) {
        cout << "\t" << kv.first << " Tags: " << std::setw(int_width) << kv.second << " (" << std::setw(flt_width) << std::fixed << (float) kv.second / total_clock_tags << ")" << endl;
    }
}

void print_timing_tags(const TimingGraph& tg, SerialTimingAnalyzer& analyzer) {
    cout << std::scientific;
    for(NodeId i = 0; i < tg.num_nodes(); i++) {
        cout << "Node: " << i << endl;;
        for(const TimingTag& tag : analyzer.data_tags(i)) {
            cout << "\tData: ";
            cout << "  clk: " << tag.clock_domain();
            cout << "  Arr: " << tag.arr_time().value();
            cout << "  Req: " << tag.req_time().value();
            cout << endl;
        }
        for(const TimingTag& tag : analyzer.clock_tags(i)) {
            cout << "\tClock: ";
            cout << "  clk: " << tag.clock_domain();
            cout << "  Arr: " << tag.arr_time().value();
            cout << "  Req: " << tag.req_time().value();
            cout << endl;
        }
    }
}


std::set<NodeId> identify_constant_gen_fanout(const TimingGraph& tg) {
    //Walk the timing graph and identify nodes that are in the fanout of a constant generator
    std::set<NodeId> const_gen_fanout_nodes;
    for(NodeId node_id : tg.primary_inputs()) {
        if(tg.node_type(node_id) == TN_Type::CONSTANT_GEN_SOURCE) {
            identify_constant_gen_fanout_helper(tg, node_id, const_gen_fanout_nodes);
        }
    }
    return const_gen_fanout_nodes;
}

void identify_constant_gen_fanout_helper(const TimingGraph& tg, const NodeId node_id, std::set<NodeId>& const_gen_fanout_nodes) {
    if(const_gen_fanout_nodes.count(node_id) == 0) {
        //Haven't seen this node before
        const_gen_fanout_nodes.insert(node_id);
        for(int edge_idx = 0; edge_idx < tg.num_node_out_edges(node_id); edge_idx++) {
            EdgeId edge_id = tg.node_out_edge(node_id, edge_idx);
            identify_constant_gen_fanout_helper(tg, tg.edge_sink_node(edge_id), const_gen_fanout_nodes);
        }
    }
}
