/*******************************************************************************
 * This file is part of KaHyPar.
 *
 * Copyright (C) 2024 KaHyPar Contributors
 *
 * KaHyPar is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * KaHyPar is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with KaHyPar.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

#include "gmock/gmock.h"

#include "kahypar/definitions.h"
#include "kahypar/partition/context.h"
#include "kahypar/partition/metrics.h"
#include "kahypar/partition/tob_utils.h"

using ::testing::Test;
using ::testing::Eq;

namespace kahypar {
namespace metrics {

// ==================== Unit Tests for Topological Level Storage ====================

class AHypergraphWithTopologicalLevels : public Test {
 public:
  AHypergraphWithTopologicalLevels() :
    // Simple hypergraph: 5 nodes, 2 hyperedges
    // HE0: {0, 1, 2}, HE1: {2, 3, 4}
    hypergraph(5, 2, HyperedgeIndexVector { 0, 3, /*sentinel*/ 6 },
               HyperedgeVector { 0, 1, 2, 2, 3, 4 }, 2) {
    // Set topological levels: 0->1->2->3->4 (linear chain)
    std::vector<int32_t> levels = {0, 1, 2, 3, 4};
    hypergraph.setTopologicalLevels(levels);
  }

  Hypergraph hypergraph;
};

TEST_F(AHypergraphWithTopologicalLevels, HasTopologicalLevels) {
  ASSERT_TRUE(hypergraph.hasTopologicalLevels());
}

TEST_F(AHypergraphWithTopologicalLevels, ReturnsCorrectTopologicalLevels) {
  ASSERT_THAT(hypergraph.topologicalLevel(0), Eq(0));
  ASSERT_THAT(hypergraph.topologicalLevel(1), Eq(1));
  ASSERT_THAT(hypergraph.topologicalLevel(2), Eq(2));
  ASSERT_THAT(hypergraph.topologicalLevel(3), Eq(3));
  ASSERT_THAT(hypergraph.topologicalLevel(4), Eq(4));
}

// ==================== Unit Tests for TOB Metric Computation ====================

class APartitionedHypergraphWithTopologicalLevels : public Test {
 public:
  APartitionedHypergraphWithTopologicalLevels() :
    // Simple hypergraph: 6 nodes, 2 hyperedges
    // HE0: {0, 1, 2}, HE1: {3, 4, 5}
    hypergraph(6, 2, HyperedgeIndexVector { 0, 3, /*sentinel*/ 6 },
               HyperedgeVector { 0, 1, 2, 3, 4, 5 }, 2) {
    // Set topological levels: nodes have levels 0, 1, 2, 3, 4, 5
    std::vector<int32_t> levels = {0, 1, 2, 3, 4, 5};
    hypergraph.setTopologicalLevels(levels);
    
    // Partition: nodes 0, 1, 2 in part 0; nodes 3, 4, 5 in part 1
    hypergraph.setNodePart(0, 0);
    hypergraph.setNodePart(1, 0);
    hypergraph.setNodePart(2, 0);
    hypergraph.setNodePart(3, 1);
    hypergraph.setNodePart(4, 1);
    hypergraph.setNodePart(5, 1);
    hypergraph.initializeNumCutHyperedges();
  }

  Hypergraph hypergraph;
};

TEST_F(APartitionedHypergraphWithTopologicalLevels, ComputesCorrectTOBMetric) {
  // For k=2 and one node per tau level, each tau contributes 0.25 to TOB.
  // 6 levels => 1.5, scaled by 1e9 => 1,500,000,000.
  ASSERT_THAT(topologyDifference(hypergraph), Eq(1500000000));
}

class AHypergraphWithMixedPartitionLevels : public Test {
 public:
  AHypergraphWithMixedPartitionLevels() :
    // 4 nodes, 2 hyperedges
    hypergraph(4, 2, HyperedgeIndexVector { 0, 2, /*sentinel*/ 4 },
               HyperedgeVector { 0, 1, 2, 3 }, 2) {
    // Levels: 0, 5, 2, 3
    std::vector<int32_t> levels = {0, 5, 2, 3};
    hypergraph.setTopologicalLevels(levels);
    
    // Partition: nodes 0, 1 in part 0; nodes 2, 3 in part 1
    hypergraph.setNodePart(0, 0);
    hypergraph.setNodePart(1, 0);
    hypergraph.setNodePart(2, 1);
    hypergraph.setNodePart(3, 1);
    hypergraph.initializeNumCutHyperedges();
  }

  Hypergraph hypergraph;
};

TEST_F(AHypergraphWithMixedPartitionLevels, ComputesCorrectTOBMetricWithMixedLevels) {
  // For k=2 and one node per tau level over 4 levels, TOB is 1.0 scaled by 1e9.
  ASSERT_THAT(topologyDifference(hypergraph), Eq(1000000000));
}

// ==================== Unit Tests for O(1) TOB Gain Computation ====================

class AHypergraphForTOBGainComputation : public Test {
 public:
  AHypergraphForTOBGainComputation() :
    // 6 nodes, 3 hyperedges
    hypergraph(6, 3, HyperedgeIndexVector { 0, 2, 4, /*sentinel*/ 6 },
               HyperedgeVector { 0, 1, 2, 3, 4, 5 }, 2) {
    // Levels: 0, 1, 2, 3, 4, 5
    std::vector<int32_t> levels = {0, 1, 2, 3, 4, 5};
    hypergraph.setTopologicalLevels(levels);
    
    // Initial partition: all in part 0
    for (HypernodeID hn = 0; hn < 6; ++hn) {
      hypergraph.setNodePart(hn, 0);
    }
    hypergraph.initializeNumCutHyperedges();
    hypergraph.initializePartLevelInfo();
  }

  Hypergraph hypergraph;
};

TEST_F(AHypergraphForTOBGainComputation, ComputesCorrectGainForMovingMinLevelNode) {
  // With one node at tau=0 (total=1), moving the only node from one block to
  // another leaves variance unchanged for that tau (gain 0).
  Gain gain = hypergraph.computeTOBGain(0, 0, 1);
  ASSERT_THAT(gain, Eq(0));
}

TEST_F(AHypergraphForTOBGainComputation, ComputesCorrectGainForMovingMiddleNode) {
  // Moving node 3 (level 3) to part 1:
  // - Part 0 would have levels {0,1,2,4,5}, range = 5 (unchanged)
  // - Part 1 would have level {3}, range = 0
  // Old total = 5 + 0 = 5
  // New total = 5 + 0 = 5
  // Gain = 0 (no change)
  
  Gain gain = hypergraph.computeTOBGain(3, 0, 1);
  ASSERT_THAT(gain, Eq(0));
}

TEST(MetricOrdering, PrefersFeasibleLowerTOBAndCutTieBreaker) {
  ASSERT_TRUE(metrics::isBetterPartition(4, 4, 8, 10, 0.0, 0.0, 0.03));
  ASSERT_TRUE(metrics::isBetterPartition(5, 6, 100, 0, 0.0, 0.0, 0.03));
  ASSERT_TRUE(metrics::isBetterPartition(8, 5, 1, 1, 0.01, 0.20, 0.03));
  ASSERT_FALSE(metrics::isBetterPartition(6, 5, 1, 1, 0.0, 0.0, 0.03));
}

// ==================== Unit Tests for PartLevelInfo Maintenance ====================

class AHypergraphWithPartLevelInfo : public Test {
 public:
  AHypergraphWithPartLevelInfo() :
    hypergraph(4, 2, HyperedgeIndexVector { 0, 2, /*sentinel*/ 4 },
               HyperedgeVector { 0, 1, 2, 3 }, 2) {
    std::vector<int32_t> levels = {0, 2, 1, 3};
    hypergraph.setTopologicalLevels(levels);
    
    // Partition: nodes 0, 1 in part 0; nodes 2, 3 in part 1
    hypergraph.setNodePart(0, 0);
    hypergraph.setNodePart(1, 0);
    hypergraph.setNodePart(2, 1);
    hypergraph.setNodePart(3, 1);
    hypergraph.initializeNumCutHyperedges();
    hypergraph.initializePartLevelInfo();
  }

  Hypergraph hypergraph;
};

TEST_F(AHypergraphWithPartLevelInfo, InitializesPartLevelInfoCorrectly) {
  // Part 0: levels {0, 2} -> min=0, max=2
  // Part 1: levels {1, 3} -> min=1, max=3
  const auto& info0 = hypergraph.partLevelInfo(0);
  const auto& info1 = hypergraph.partLevelInfo(1);
  
  ASSERT_THAT(info0.min_level, Eq(0));
  ASSERT_THAT(info0.max_level, Eq(2));
  ASSERT_THAT(info1.min_level, Eq(1));
  ASSERT_THAT(info1.max_level, Eq(3));
}

// ==================== Integration Tests ====================

class TOBObjectiveIntegrationTest : public Test {
 public:
  TOBObjectiveIntegrationTest() :
    context() {
    context.partition.k = 2;
    context.partition.objective = Objective::tob;
    context.partition.mode = Mode::direct_kway;
    context.partition.epsilon = 0.03;
  }

  Context context;
};

TEST_F(TOBObjectiveIntegrationTest, MetricsStructureHandlesTOB) {
  Metrics metrics;
  metrics.cut = 0;
  metrics.km1 = 0;
  metrics.tob = 10;
  metrics.imbalance = 0.0;
  
  // Test updateMetric for TOB
  metrics.updateMetric(5, Mode::direct_kway, Objective::tob);
  ASSERT_THAT(metrics.tob, Eq(5));
  
  // Test getMetric for TOB
  HyperedgeWeight tob_value = metrics.getMetric(Mode::direct_kway, Objective::tob);
  ASSERT_THAT(tob_value, Eq(5));
}

TEST_F(TOBObjectiveIntegrationTest, ObjectiveFunctionReturnsTOB) {
  // Create a simple hypergraph with topological levels
  Hypergraph hypergraph(4, 2, HyperedgeIndexVector { 0, 2, /*sentinel*/ 4 },
                        HyperedgeVector { 0, 1, 2, 3 }, 2);
  
  std::vector<int32_t> levels = {0, 1, 2, 3};
  hypergraph.setTopologicalLevels(levels);
  
  hypergraph.setNodePart(0, 0);
  hypergraph.setNodePart(1, 0);
  hypergraph.setNodePart(2, 1);
  hypergraph.setNodePart(3, 1);
  hypergraph.initializeNumCutHyperedges();
  
  // 4 tau levels with one node each for k=2 => 1.0 scaled by 1e9.
  HyperedgeWeight tob = objective(hypergraph, Objective::tob);
  ASSERT_THAT(tob, Eq(1000000000));
}

// ==================== Edge Case Tests ====================

class TOBEdgeCaseTests : public Test {
 public:
  TOBEdgeCaseTests() {}
};

TEST_F(TOBEdgeCaseTests, HypergraphWithoutTopologicalLevelsReturnsTOBZero) {
  Hypergraph hypergraph(4, 2, HyperedgeIndexVector { 0, 2, /*sentinel*/ 4 },
                        HyperedgeVector { 0, 1, 2, 3 }, 2);
  
  // No topological levels set
  hypergraph.setNodePart(0, 0);
  hypergraph.setNodePart(1, 0);
  hypergraph.setNodePart(2, 1);
  hypergraph.setNodePart(3, 1);
  hypergraph.initializeNumCutHyperedges();
  
  ASSERT_FALSE(hypergraph.hasTopologicalLevels());
  ASSERT_THAT(topologyDifference(hypergraph), Eq(0));
}

TEST_F(TOBEdgeCaseTests, SingleNodePartitionHasTOBZero) {
  Hypergraph hypergraph(2, 1, HyperedgeIndexVector { 0, /*sentinel*/ 2 },
                        HyperedgeVector { 0, 1 }, 2);
  
  std::vector<int32_t> levels = {0, 5};
  hypergraph.setTopologicalLevels(levels);
  
  // Each partition has one node
  hypergraph.setNodePart(0, 0);
  hypergraph.setNodePart(1, 1);
  hypergraph.initializeNumCutHyperedges();
  
  // Each tau appears once, so each tau contributes 0.25 for k=2.
  // 2 levels => 0.5 scaled by 1e9.
  ASSERT_THAT(topologyDifference(hypergraph), Eq(500000000));
}

TEST_F(TOBEdgeCaseTests, AllNodesInSamePartition) {
  Hypergraph hypergraph(4, 2, HyperedgeIndexVector { 0, 2, /*sentinel*/ 4 },
                        HyperedgeVector { 0, 1, 2, 3 }, 2);
  
  std::vector<int32_t> levels = {0, 3, 1, 2};
  hypergraph.setTopologicalLevels(levels);
  
  // All nodes in part 0
  for (HypernodeID hn = 0; hn < 4; ++hn) {
    hypergraph.setNodePart(hn, 0);
  }
  hypergraph.initializeNumCutHyperedges();
  
  // For 4 unique levels with k=2 and all nodes in one block, TOB is 1.0 scaled.
  ASSERT_THAT(topologyDifference(hypergraph), Eq(1000000000));
}

TEST_F(TOBEdgeCaseTests, AllNodesHaveSameLevel) {
  Hypergraph hypergraph(4, 2, HyperedgeIndexVector { 0, 2, /*sentinel*/ 4 },
                        HyperedgeVector { 0, 1, 2, 3 }, 2);
  
  std::vector<int32_t> levels = {5, 5, 5, 5};
  hypergraph.setTopologicalLevels(levels);
  
  hypergraph.setNodePart(0, 0);
  hypergraph.setNodePart(1, 0);
  hypergraph.setNodePart(2, 1);
  hypergraph.setNodePart(3, 1);
  hypergraph.initializeNumCutHyperedges();
  
  // All nodes have level 5, so range in each partition is 0
  ASSERT_THAT(topologyDifference(hypergraph), Eq(0));
}

}  // namespace metrics
}  // namespace kahypar
