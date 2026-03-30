/*******************************************************************************
 * TOB vs Cut Objective Comparison Test
 * 
 * This program creates a synthetic DAG hypergraph, partitions it using both
 * TOB (Topological Order Balancing) and min-cut objectives, and compares
 * the results to demonstrate the effectiveness of the TOB implementation.
 ******************************************************************************/

#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <iomanip>
#include <chrono>
#include <cmath>

#include "kahypar/definitions.h"
#include "kahypar/io/hypergraph_io.h"
#include "kahypar/partition/context.h"
#include "kahypar/partition/metrics.h"
#include "kahypar/partitioner_facade.h"
#include "kahypar-resources/utils/randomize.h"

using namespace kahypar;

// Generate a synthetic DAG hypergraph representing a layered circuit/dataflow
// This simulates a real netlist with topological structure
Hypergraph generateLayeredDAGHypergraph(
    const HypernodeID num_layers,
    const HypernodeID nodes_per_layer,
    const HyperedgeID edges_per_layer,
    const PartitionID k,
    std::vector<int32_t>& topological_levels) {
  
  const HypernodeID num_nodes = num_layers * nodes_per_layer;
  HyperedgeIndexVector index_vector;
  HyperedgeVector edge_vector;
  
  std::mt19937 rng(42);  // Fixed seed for reproducibility
  
  index_vector.push_back(0);
  
  // Create hyperedges connecting nodes between adjacent layers
  // This simulates signal propagation in a circuit
  for (HypernodeID layer = 0; layer < num_layers - 1; ++layer) {
    for (HyperedgeID e = 0; e < edges_per_layer; ++e) {
      // Select 1-2 source nodes from current layer
      HypernodeID src1 = layer * nodes_per_layer + (rng() % nodes_per_layer);
      
      // Select 1-3 target nodes from next layer
      HypernodeID tgt1 = (layer + 1) * nodes_per_layer + (rng() % nodes_per_layer);
      HypernodeID tgt2 = (layer + 1) * nodes_per_layer + (rng() % nodes_per_layer);
      
      edge_vector.push_back(src1);
      edge_vector.push_back(tgt1);
      if (tgt2 != tgt1) {
        edge_vector.push_back(tgt2);
      }
      
      // Sometimes add another source
      if (rng() % 3 == 0) {
        HypernodeID src2 = layer * nodes_per_layer + (rng() % nodes_per_layer);
        if (src2 != src1) {
          edge_vector.push_back(src2);
        }
      }
      
      index_vector.push_back(edge_vector.size());
    }
  }
  
  // Add some cross-layer edges (skip connections) to make it more realistic
  for (HypernodeID layer = 0; layer < num_layers - 2; ++layer) {
    for (HyperedgeID e = 0; e < edges_per_layer / 4; ++e) {
      HypernodeID src = layer * nodes_per_layer + (rng() % nodes_per_layer);
      HypernodeID tgt = (layer + 2) * nodes_per_layer + (rng() % nodes_per_layer);
      
      edge_vector.push_back(src);
      edge_vector.push_back(tgt);
      index_vector.push_back(edge_vector.size());
    }
  }
  
  const HyperedgeID num_edges = index_vector.size() - 1;
  
  // Set topological levels based on layer structure
  topological_levels.resize(num_nodes);
  for (HypernodeID hn = 0; hn < num_nodes; ++hn) {
    topological_levels[hn] = static_cast<int32_t>(hn / nodes_per_layer);
  }
  
  Hypergraph hypergraph(num_nodes, num_edges, index_vector, edge_vector, k);
  hypergraph.setTopologicalLevels(topological_levels);
  
  return hypergraph;
}

// Copy partition from one hypergraph to another
void copyPartition(const Hypergraph& src, Hypergraph& dst) {
  for (const HypernodeID& hn : src.nodes()) {
    dst.setNodePart(hn, src.partID(hn));
  }
  dst.initializeNumCutHyperedges();
}

// Compute detailed TOB statistics per partition
void printTOBStatistics(const Hypergraph& hg, const std::string& label) {
  std::cout << "\n=== " << label << " ===" << std::endl;
  std::cout << std::setw(10) << "Part" 
            << std::setw(12) << "Size"
            << std::setw(12) << "MinLevel"
            << std::setw(12) << "MaxLevel"
            << std::setw(12) << "Range"
            << std::endl;
  std::cout << std::string(58, '-') << std::endl;
  
  HyperedgeWeight total_tob = 0;
  for (PartitionID part = 0; part < hg.k(); ++part) {
    int32_t min_level = std::numeric_limits<int32_t>::max();
    int32_t max_level = std::numeric_limits<int32_t>::min();
    HypernodeID size = 0;
    
    for (const HypernodeID& hn : hg.nodes()) {
      if (hg.partID(hn) == part) {
        const int32_t level = hg.topologicalLevel(hn);
        min_level = std::min(min_level, level);
        max_level = std::max(max_level, level);
        ++size;
      }
    }
    
    int32_t range = (size > 0) ? (max_level - min_level) : 0;
    total_tob += range;
    
    if (size > 0) {
      std::cout << std::setw(10) << part
                << std::setw(12) << size
                << std::setw(12) << min_level
                << std::setw(12) << max_level
                << std::setw(12) << range
                << std::endl;
    }
  }
  
  std::cout << std::string(58, '-') << std::endl;
  std::cout << "Range TOB (legacy-style): " << total_tob << std::endl;
  std::cout << "Variance TOB (scaled): " << metrics::topologyDifference(hg) << std::endl;
  std::cout << "Cut: " << metrics::hyperedgeCut(hg) << std::endl;
  std::cout << "km1: " << metrics::km1(hg) << std::endl;
  std::cout << "Imbalance: " << std::fixed << std::setprecision(4);
  
  HypernodeWeight max_weight = 0;
  for (PartitionID p = 0; p < hg.k(); ++p) {
    max_weight = std::max(max_weight, hg.partWeight(p));
  }
  double imbalance = static_cast<double>(max_weight) / 
                     std::ceil(static_cast<double>(hg.totalWeight()) / hg.k()) - 1.0;
  std::cout << imbalance << std::endl;
}

void setupContext(Context& context, const Hypergraph& hg, Objective objective) {
  context.partition.k = hg.k();
  context.partition.epsilon = 0.03;
  context.partition.objective = objective;
  context.partition.mode = Mode::direct_kway;
  context.partition.seed = 42;
  
  // Setup coarsening
  context.coarsening.algorithm = CoarseningAlgorithm::ml_style;
  context.coarsening.max_allowed_weight_multiplier = 1.0;
  context.coarsening.contraction_limit_multiplier = 160;
  context.coarsening.rating.rating_function = RatingFunction::heavy_edge;
  context.coarsening.rating.community_policy = CommunityPolicy::use_communities;
  context.coarsening.rating.heavy_node_penalty_policy = HeavyNodePenaltyPolicy::multiplicative_penalty;
  context.coarsening.rating.acceptance_policy = AcceptancePolicy::best_prefer_unmatched;
  
  // Setup initial partitioning
  context.initial_partitioning.mode = Mode::direct_kway;
  context.initial_partitioning.technique = InitialPartitioningTechnique::flat;
  context.initial_partitioning.algo = InitialPartitionerAlgorithm::pool;
  context.initial_partitioning.nruns = 1;
  context.initial_partitioning.coarsening.algorithm = CoarseningAlgorithm::ml_style;
  context.initial_partitioning.coarsening.max_allowed_weight_multiplier = 1.0;
  context.initial_partitioning.coarsening.contraction_limit_multiplier = 150;
  context.initial_partitioning.local_search.algorithm = RefinementAlgorithm::kway_fm;
  context.initial_partitioning.local_search.fm.stopping_rule = RefinementStoppingRule::simple;
  context.initial_partitioning.local_search.fm.max_number_of_fruitless_moves = 50;
  
  // Setup refinement
  context.local_search.algorithm = RefinementAlgorithm::kway_fm;
  context.local_search.fm.stopping_rule = RefinementStoppingRule::simple;
  context.local_search.fm.max_number_of_fruitless_moves = 350;
  context.local_search.iterations_per_level = -1;  // unlimited
  
  context.partition.verbose_output = false;
  context.partition.quiet_mode = true;
  context.partition.sp_process_output = false;

  if (objective == Objective::tob) {
    context.initial_partitioning.algo = InitialPartitionerAlgorithm::tob_super_far;
    context.initial_partitioning.local_search.algorithm = RefinementAlgorithm::tob_refine;
    context.local_search.algorithm = RefinementAlgorithm::tob_refine;
  }

  // Avoid interactive prompts by explicitly enabling individual block weights.
  const HypernodeWeight l_opt = std::ceil(static_cast<double>(hg.totalWeight()) / hg.k());
  const HypernodeWeight l_max = static_cast<HypernodeWeight>(
    std::ceil((1.0 + context.partition.epsilon) * l_opt));
  context.partition.use_individual_part_weights = true;
  context.partition.perfect_balance_part_weights.assign(hg.k(), l_opt);
  context.partition.max_part_weights.assign(hg.k(), l_max);
  context.initial_partitioning.perfect_balance_partition_weight.assign(hg.k(), l_opt);
  context.initial_partitioning.upper_allowed_partition_weight.assign(hg.k(), l_max);
}

int main(int argc, char* argv[]) {
  std::cout << "╔══════════════════════════════════════════════════════════════════╗" << std::endl;
  std::cout << "║     TOB (Topological Order Balancing) vs Min-Cut Comparison      ║" << std::endl;
  std::cout << "╚══════════════════════════════════════════════════════════════════╝" << std::endl;
  
  // Parameters for the synthetic DAG
  const HypernodeID num_layers = 20;
  const HypernodeID nodes_per_layer = 50;
  const HyperedgeID edges_per_layer = 80;
  const PartitionID k = 4;
  
  std::cout << "\n[1] Generating synthetic layered DAG hypergraph..." << std::endl;
  std::cout << "    - Layers: " << num_layers << std::endl;
  std::cout << "    - Nodes per layer: " << nodes_per_layer << std::endl;
  std::cout << "    - Total nodes: " << num_layers * nodes_per_layer << std::endl;
  std::cout << "    - Partitions (k): " << k << std::endl;
  
  std::vector<int32_t> topological_levels;
  Hypergraph hg_tob = generateLayeredDAGHypergraph(num_layers, nodes_per_layer, 
                                                    edges_per_layer, k, topological_levels);
  
  std::cout << "    - Hyperedges: " << hg_tob.initialNumEdges() << std::endl;
  std::cout << "    - Pins: " << hg_tob.initialNumPins() << std::endl;
  std::cout << "    - Topological levels: 0 to " << (num_layers - 1) << std::endl;
  
  // Create a copy for cut-based partitioning
  Hypergraph hg_cut = generateLayeredDAGHypergraph(num_layers, nodes_per_layer,
                                                    edges_per_layer, k, topological_levels);
  
  // ========== Partition with TOB objective ==========
  std::cout << "\n[2] Partitioning with TOB objective..." << std::endl;
  Context context_tob;
  setupContext(context_tob, hg_tob, Objective::tob);
  
  auto start_tob = std::chrono::high_resolution_clock::now();
  PartitionerFacade().partition(hg_tob, context_tob);
  auto end_tob = std::chrono::high_resolution_clock::now();
  double time_tob = std::chrono::duration<double>(end_tob - start_tob).count();
  
  std::cout << "    Time: " << std::fixed << std::setprecision(3) << time_tob << "s" << std::endl;
  
  // ========== Partition with Cut objective ==========
  std::cout << "\n[3] Partitioning with Cut objective..." << std::endl;
  Context context_cut;
  setupContext(context_cut, hg_cut, Objective::cut);
  
  auto start_cut = std::chrono::high_resolution_clock::now();
  PartitionerFacade().partition(hg_cut, context_cut);
  auto end_cut = std::chrono::high_resolution_clock::now();
  double time_cut = std::chrono::duration<double>(end_cut - start_cut).count();
  
  std::cout << "    Time: " << std::fixed << std::setprecision(3) << time_cut << "s" << std::endl;
  
  // ========== Print detailed statistics ==========
  std::cout << "\n" << std::string(70, '=') << std::endl;
  std::cout << "                    PARTITIONING RESULTS COMPARISON" << std::endl;
  std::cout << std::string(70, '=') << std::endl;
  
  printTOBStatistics(hg_tob, "TOB Objective Partitioning");
  printTOBStatistics(hg_cut, "Cut Objective Partitioning");
  
  // ========== Summary comparison ==========
  HyperedgeWeight tob_tob = metrics::topologyDifference(hg_tob);
  HyperedgeWeight tob_cut = metrics::topologyDifference(hg_cut);
  HyperedgeWeight cut_tob = metrics::hyperedgeCut(hg_tob);
  HyperedgeWeight cut_cut = metrics::hyperedgeCut(hg_cut);
  
  std::cout << "\n" << std::string(70, '=') << std::endl;
  std::cout << "                         SUMMARY COMPARISON" << std::endl;
  std::cout << std::string(70, '=') << std::endl;
  
  std::cout << "\n┌─────────────────────┬─────────────────┬─────────────────┐" << std::endl;
  std::cout << "│      Metric         │  TOB Objective  │  Cut Objective  │" << std::endl;
  std::cout << "├─────────────────────┼─────────────────┼─────────────────┤" << std::endl;
  std::cout << "│ TOB (lower=better)  │" << std::setw(16) << tob_tob << " │" 
            << std::setw(16) << tob_cut << " │" << std::endl;
  std::cout << "│ Cut (lower=better)  │" << std::setw(16) << cut_tob << " │" 
            << std::setw(16) << cut_cut << " │" << std::endl;
  std::cout << "│ Time (seconds)      │" << std::setw(16) << std::fixed << std::setprecision(3) << time_tob << " │" 
            << std::setw(16) << time_cut << " │" << std::endl;
  std::cout << "└─────────────────────┴─────────────────┴─────────────────┘" << std::endl;
  
  // Calculate improvements
  double tob_improvement = 100.0 * (tob_cut - tob_tob) / static_cast<double>(tob_cut);
  double cut_degradation = 100.0 * (cut_tob - cut_cut) / static_cast<double>(cut_cut);
  
  std::cout << "\n┌──────────��──────────────────────────────────────────────────────┐" << std::endl;
  std::cout << "│                      EFFECTIVENESS ANALYSIS                     │" << std::endl;
  std::cout << "├─────────────────────────────────────────────────────────────────┤" << std::endl;
  
  if (tob_improvement > 0) {
    std::cout << "│ ✓ TOB objective achieves " << std::fixed << std::setprecision(1) 
              << std::setw(5) << tob_improvement << "% better TOB metric            │" << std::endl;
  } else {
    std::cout << "│ ✗ TOB objective achieves " << std::fixed << std::setprecision(1) 
              << std::setw(5) << -tob_improvement << "% worse TOB metric             │" << std::endl;
  }
  
  if (cut_degradation > 0) {
    std::cout << "│ • Cut metric degradation: " << std::fixed << std::setprecision(1) 
              << std::setw(5) << cut_degradation << "% (expected trade-off)        │" << std::endl;
  } else {
    std::cout << "│ ✓ Cut metric also improved by " << std::fixed << std::setprecision(1) 
              << std::setw(5) << -cut_degradation << "%                       │" << std::endl;
  }
  
  std::cout << "└─────────────────────────────────────────────────────────────────┘" << std::endl;
  
  std::cout << "\n[CONCLUSION]" << std::endl;
  if (tob_improvement > 0) {
    std::cout << "The TOB objective successfully minimizes topological spread within" << std::endl;
    std::cout << "partitions, which is crucial for DAG scheduling and pipelining." << std::endl;
    std::cout << "This demonstrates that the TOB implementation is effective!" << std::endl;
  } else {
    std::cout << "The results show similar TOB metrics. This may occur when the" << std::endl;
    std::cout << "hypergraph structure naturally leads to good topological grouping." << std::endl;
  }
  
  return 0;
}
