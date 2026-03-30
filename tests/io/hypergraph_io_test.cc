/*******************************************************************************
 * This file is part of KaHyPar.
 *
 * Copyright (C) 2014 Sebastian Schlag <sebastian.schlag@kit.edu>
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

#include "kahypar/io/hypergraph_io.h"
#include "tests/io/hypergraph_io_test_fixtures.h"

using ::testing::Eq;
using ::testing::ContainerEq;

namespace kahypar {
namespace io {
TEST(AFunction, ParsesFirstLineOfaHGRFile) {
  std::string filename("test_instances/unweighted_hypergraph.hgr");
  std::ifstream file(filename);
  HyperedgeID num_hyperedges = 0;
  HypernodeID num_hypernodes = 0;
  HypergraphType hypergraph_type = HypergraphType::Unweighted;
  size_t line_number = 0;

  readHGRHeader(file, num_hyperedges, num_hypernodes, hypergraph_type, line_number);
  ASSERT_THAT(num_hyperedges, Eq(4));
  ASSERT_THAT(num_hypernodes, Eq(7));
  ASSERT_THAT(hypergraph_type, Eq(HypergraphType::Unweighted));
}

TEST_F(AnUnweightedHypergraphFile, CanBeParsedIntoAHypergraph) {
  HyperedgeIndexVector index_vector;
  HyperedgeVector edge_vector;

  readHypergraphFile(_filename, _num_hypernodes, _num_hyperedges, index_vector, edge_vector);

  ASSERT_THAT(index_vector, ContainerEq(_control_index_vector));
  ASSERT_THAT(edge_vector, ContainerEq(_control_edge_vector));
  Hypergraph hypergraph(_num_hypernodes, _num_hyperedges, index_vector, edge_vector);
}

TEST_F(AHypergraphFileWithHyperedgeWeights, CanBeParsedIntoAHypergraph) {
  HyperedgeIndexVector index_vector;
  HyperedgeVector edge_vector;
  HyperedgeWeightVector hyperedge_weights;

  readHypergraphFile(_filename, _num_hypernodes, _num_hyperedges, index_vector, edge_vector,
                     &hyperedge_weights);

  ASSERT_THAT(index_vector, ContainerEq(_control_index_vector));
  ASSERT_THAT(edge_vector, ContainerEq(_control_edge_vector));
  ASSERT_THAT(hyperedge_weights, ContainerEq(_control_hyperedge_weights));
  Hypergraph hypergraph(_num_hypernodes, _num_hyperedges, index_vector, edge_vector,
                        2, &hyperedge_weights);
}

TEST_F(AHypergraphFileWithHypernodeWeights, CanBeParsedIntoAHypergraph) {
  HyperedgeIndexVector index_vector;
  HyperedgeVector edge_vector;
  HypernodeWeightVector hypernode_weights;

  readHypergraphFile(_filename, _num_hypernodes, _num_hyperedges, index_vector, edge_vector,
                     nullptr, &hypernode_weights);

  ASSERT_THAT(index_vector, ContainerEq(_control_index_vector));
  ASSERT_THAT(edge_vector, ContainerEq(_control_edge_vector));
  // ASSERT_THAT(hypernode_weights, ContainerEq(_control_hypernode_weights));
  Hypergraph hypergraph(_num_hypernodes, _num_hyperedges, index_vector, edge_vector,
                        2, nullptr, &hypernode_weights);
}

TEST_F(AHypergraphFileWithHypernodeAndHyperedgeWeights, CanBeParsedIntoAHypergraph) {
  HyperedgeIndexVector index_vector;
  HyperedgeVector edge_vector;
  HypernodeWeightVector hypernode_weights;
  HyperedgeWeightVector hyperedge_weights;

  readHypergraphFile(_filename, _num_hypernodes, _num_hyperedges, index_vector, edge_vector,
                     &hyperedge_weights, &hypernode_weights);

  ASSERT_THAT(index_vector, ContainerEq(_control_index_vector));
  ASSERT_THAT(edge_vector, ContainerEq(_control_edge_vector));
  ASSERT_THAT(hyperedge_weights, ContainerEq(_control_hyperedge_weights));
  ASSERT_THAT(hypernode_weights, ContainerEq(_control_hypernode_weights));
  Hypergraph hypergraph(_num_hypernodes, _num_hyperedges, index_vector, edge_vector,
                        2, &hyperedge_weights, &hypernode_weights);
}

TEST_F(AHypergraphFileWithoutHyperedges, CanBeParsedIntoAHypergraphIfFileContainesHypernodeWeights) {
  HyperedgeIndexVector index_vector;
  HyperedgeVector edge_vector;
  HypernodeWeightVector hypernode_weights;
  HyperedgeWeightVector hyperedge_weights;
  readHypergraphFile(_filename, _num_hypernodes, _num_hyperedges, index_vector, edge_vector,
                     &hyperedge_weights, &hypernode_weights);
  ASSERT_THAT(index_vector.size(), Eq(1));
  ASSERT_THAT(edge_vector.empty(), Eq(true));
  ASSERT_THAT(_num_hypernodes, Eq(3));
  ASSERT_THAT(_num_hyperedges, Eq(0));
  ASSERT_THAT(hypernode_weights, ::testing::ContainerEq(_control_hypernode_weights));
  ASSERT_THAT(hyperedge_weights.empty(), Eq(true));

  Hypergraph hypergraph(_num_hypernodes, _num_hyperedges, index_vector, edge_vector,
                        2, &hyperedge_weights, &hypernode_weights);
  ASSERT_THAT(hypergraph.initialNumNodes(), Eq(3));
  ASSERT_THAT(hypergraph.currentNumEdges(), Eq(0));
  for (const HypernodeID& hn : hypergraph.nodes()) {
    ASSERT_THAT(hypergraph.nodeWeight(hn), Eq(_control_hypernode_weights[hn]));
  }
}

TEST(AHypergraphWithoutHyperedges, CanBeWrittenToFile) {
  HypernodeID num_hypernodes = 0;
  HyperedgeID num_hyperedges = 0;
  HyperedgeIndexVector index_vector;
  HyperedgeVector edge_vector;
  HypernodeWeightVector hypernode_weights;
  HyperedgeWeightVector hyperedge_weights;
  std::string input_filename("test_instances/hypergraph_without_hyperedges.hgr");
  std::string output_filename("test_instances/hypergraph_without_hyperedges.hgr.out");
  readHypergraphFile(input_filename, num_hypernodes, num_hyperedges, index_vector, edge_vector,
                     &hyperedge_weights, &hypernode_weights);
  Hypergraph hypergraph(num_hypernodes, num_hyperedges, index_vector, edge_vector,
                        2, &hyperedge_weights, &hypernode_weights);
  num_hypernodes = 0;
  num_hyperedges = 0;
  index_vector.clear();
  edge_vector.clear();
  hypernode_weights.clear();
  hyperedge_weights.clear();


  writeHypergraphFile(hypergraph, output_filename);
  readHypergraphFile(output_filename, num_hypernodes, num_hyperedges, index_vector, edge_vector,
                     &hyperedge_weights, &hypernode_weights);
  Hypergraph hypergraph_from_file(num_hypernodes, num_hyperedges, index_vector, edge_vector,
                                  2, &hyperedge_weights, &hypernode_weights);

  ASSERT_THAT(verifyEquivalenceWithPartitionInfo(hypergraph, hypergraph_from_file), Eq(true));
}

TEST_F(AnUnweightedHypergraph, CanBeWrittenToFile) {
  writeHypergraphFile(*_hypergraph, _filename);

  readHypergraphFile(_filename, _num_hypernodes, _num_hyperedges, _written_index_vector,
                     _written_edge_vector);
  Hypergraph hypergraph2(_num_hypernodes, _num_hyperedges, _written_index_vector,
                         _written_edge_vector);

  ASSERT_THAT(verifyEquivalenceWithPartitionInfo(*_hypergraph, hypergraph2), Eq(true));
}

TEST_F(AHypergraphWithHyperedgeWeights, CanBeWrittenToFile) {
  writeHypergraphFile(*_hypergraph, _filename);

  readHypergraphFile(_filename, _num_hypernodes, _num_hyperedges, _written_index_vector,
                     _written_edge_vector, &_written_hyperedge_weights, nullptr);
  Hypergraph hypergraph2(_num_hypernodes, _num_hyperedges, _written_index_vector,
                         _written_edge_vector, 2, &_written_hyperedge_weights);
  ASSERT_THAT(verifyEquivalenceWithPartitionInfo(*_hypergraph, hypergraph2), Eq(true));
}

TEST_F(AHypergraphWithHypernodeWeights, CanBeWrittenToFile) {
  writeHypergraphFile(*_hypergraph, _filename);

  readHypergraphFile(_filename, _num_hypernodes, _num_hyperedges, _written_index_vector,
                     _written_edge_vector, nullptr, &_written_hypernode_weights);
  Hypergraph hypergraph2(_num_hypernodes, _num_hyperedges, _written_index_vector,
                         _written_edge_vector, 2, nullptr, &_written_hypernode_weights);

  ASSERT_THAT(verifyEquivalenceWithPartitionInfo(*_hypergraph, hypergraph2), Eq(true));
}

TEST_F(AHypergraphWithHypernodeAndHyperedgeWeights, CanBeWrittenToFile) {
  writeHypergraphFile(*_hypergraph, _filename);

  readHypergraphFile(_filename, _num_hypernodes, _num_hyperedges, _written_index_vector,
                     _written_edge_vector, &_written_hyperedge_weights,
                     &_written_hypernode_weights);
  Hypergraph hypergraph2(_num_hypernodes, _num_hyperedges, _written_index_vector,
                         _written_edge_vector, 2, &_written_hyperedge_weights,
                         &_written_hypernode_weights);

  ASSERT_THAT(verifyEquivalenceWithPartitionInfo(*_hypergraph, hypergraph2), Eq(true));
}

TEST_F(APartitionOfAHypergraph, IsCorrectlyWrittenToFile) {
  multilevel::partition(_hypergraph, *_coarsener, *_refiner, _context);
  writePartitionFile(_hypergraph, _context.partition.graph_partition_filename);

  std::vector<PartitionID> read_partition;
  readPartitionFile(_context.partition.graph_partition_filename, read_partition);
  for (const HypernodeID& hn : _hypergraph.nodes()) {
    ASSERT_THAT(read_partition[hn], Eq(_hypergraph.partID(hn)));
  }
}

TEST(AHypergraph, CanBeSerializedToPaToHFormat) {
  HyperedgeWeightVector he_weights = { 10, 15, 13, 18, 25, 20, 14, 27, 29 };
  HypernodeWeightVector hn_weights = HypernodeWeightVector { 80, 85, 30, 55, 42, 39, 90, 102 };
  Hypergraph hypergraph(8, 9, HyperedgeIndexVector { 0, 5, 9, 13, 15, 17, 20, 23, 26,  /*sentinel*/ 28 },
                        HyperedgeVector { 7, 5, 2, 4, 1, 3, 4, 0, 6, 3, 1, 4, 6, 3, 6, 2, 4, 7, 1, 3, 5, 4, 1, 4, 6, 1, 7, 3 },
                        2, &he_weights, &hn_weights);

  std::unordered_map<HypernodeID, HypernodeID> mapping;
  for (int i = 0; i < 8; ++i) {
    mapping[i] = i;
  }
  writeHypergraphForPaToHPartitioning(hypergraph, "serialized_hypergraph.patoh", mapping);

  std::ifstream file;
  std::vector<std::string> original_lines;
  std::vector<std::string> serialized_lines;
  std::string tmp;

  file.open("serialized_hypergraph.patoh", std::ifstream::in);
  while (getline(file, tmp)) {
    serialized_lines.push_back(std::move(tmp));
  }
  file.close();

  LOG << serialized_lines.size();

  file.open("test_instances/example_hypergraph.patoh", std::ifstream::in);
  while (getline(file, tmp)) {
    original_lines.push_back(std::move(tmp));
  }
  file.close();

  ASSERT_THAT(serialized_lines, ::testing::ContainerEq(original_lines));
}

TEST(AHypergraphDeathTest, WithEmptyHyperedgesLeadsToProgramExit) {
  EXPECT_EXIT(createHypergraphFromFile("test_instances/corrupted_hypergraph_with_empty_hyperedges.hgr", 2, true, false),
              ::testing::ExitedWithCode(1),
              ""); // "Error: Hyperedge is empty (line 3)"); --> for some reason gtest ignores the output
}

TEST(DuplicatePins, GetRemovedDuringParsing) {
  Hypergraph const hypergraph = createHypergraphFromFile("test_instances/corrupted_hypergraph_with_multiple_identical_pins.hgr",
                                                         2, true, false);
  ASSERT_THAT(hypergraph.initialNumNodes(), Eq(3));
  ASSERT_THAT(hypergraph.initialNumEdges(), Eq(2));
  ASSERT_THAT(hypergraph.initialNumPins(), Eq(4));
}

TEST(DuplicatePinsAndInvalidHes, GetRemovedDuringParsing) {
  Hypergraph const hypergraph = createHypergraphFromFile("test_instances/corrupted_hypergraph_with_identical_pins_and_invalid_edges.hgr",
                                                         2, true, false);
  ASSERT_THAT(hypergraph.initialNumNodes(), Eq(3));
  ASSERT_THAT(hypergraph.initialNumEdges(), Eq(3));
  ASSERT_THAT(hypergraph.initialNumPins(), Eq(6));
}

// ==================== Topological Level I/O Tests ====================

TEST(TopologicalLevelIO, CanWriteAndReadTopologicalLevels) {
  // Create a hypergraph with topological levels
  Hypergraph hypergraph(4, 2, HyperedgeIndexVector { 0, 2, /*sentinel*/ 4 },
                        HyperedgeVector { 0, 1, 2, 3 }, 2);
  
  std::vector<int32_t> levels = {0, 3, 1, 2};
  hypergraph.setTopologicalLevels(levels);
  
  // Write to file
  std::string filename = "test_instances/test_topological_levels.lvl";
  writeTopologicalLevelFile(hypergraph, filename);
  
  // Create a new hypergraph and read levels
  Hypergraph hypergraph2(4, 2, HyperedgeIndexVector { 0, 2, /*sentinel*/ 4 },
                         HyperedgeVector { 0, 1, 2, 3 }, 2);
  readTopologicalLevelFile(hypergraph2, filename);
  
  // Verify levels match
  ASSERT_TRUE(hypergraph2.hasTopologicalLevels());
  for (HypernodeID hn = 0; hn < 4; ++hn) {
    ASSERT_THAT(hypergraph2.topologicalLevel(hn), Eq(levels[hn]));
  }
}

TEST(TopologicalLevelIO, ComputesTopologicalLevelsFromHypergraph) {
  // Create a hypergraph where first pin is source, rest are targets
  // HE0: 0 -> 1, 2 (node 0 is source, nodes 1, 2 are targets)
  // HE1: 1 -> 3 (node 1 is source, node 3 is target)
  Hypergraph hypergraph(4, 2, HyperedgeIndexVector { 0, 3, /*sentinel*/ 5 },
                        HyperedgeVector { 0, 1, 2, 1, 3 }, 2);
  
  computeTopologicalLevelsFromHypergraph(hypergraph, 0);
  
  ASSERT_TRUE(hypergraph.hasTopologicalLevels());
  // Expected levels: 0 -> level 0, 1 -> level 1, 2 -> level 1, 3 -> level 2
  ASSERT_THAT(hypergraph.topologicalLevel(0), Eq(0));
  ASSERT_THAT(hypergraph.topologicalLevel(1), Eq(1));
  ASSERT_THAT(hypergraph.topologicalLevel(2), Eq(1));
  ASSERT_THAT(hypergraph.topologicalLevel(3), Eq(2));
}

TEST(TopologicalLevelIO, HypergraphWithoutLevelsWritesZeros) {
  Hypergraph hypergraph(3, 1, HyperedgeIndexVector { 0, /*sentinel*/ 3 },
                        HyperedgeVector { 0, 1, 2 }, 2);
  
  // No topological levels set
  ASSERT_FALSE(hypergraph.hasTopologicalLevels());
  
  std::string filename = "test_instances/test_no_levels.lvl";
  writeTopologicalLevelFile(hypergraph, filename);
  
  // Read the file and verify it contains zeros
  std::ifstream file(filename);
  int32_t level;
  int count = 0;
  while (file >> level) {
    ASSERT_THAT(level, Eq(0));
    ++count;
  }
  ASSERT_THAT(count, Eq(3));
}

TEST(TopologicalLevelIO, CreatesHypergraphFromDahInputFile) {
  Hypergraph hypergraph = createHypergraphFromInputFile("test_instances/simple_pipeline.dah", 2);

  ASSERT_TRUE(hypergraph.hasTopologicalLevels());
  ASSERT_THAT(hypergraph.initialNumNodes(), Eq(4));
  ASSERT_THAT(hypergraph.initialNumEdges(), Eq(3));
  ASSERT_THAT(hypergraph.topologicalLevel(0), Eq(0));
  ASSERT_THAT(hypergraph.topologicalLevel(1), Eq(1));
  ASSERT_THAT(hypergraph.topologicalLevel(2), Eq(2));
  ASSERT_THAT(hypergraph.topologicalLevel(3), Eq(3));
}

TEST(TopologicalLevelIO, CreatesHypergraphFromHgrAndLevelSidecar) {
  Hypergraph hypergraph = createHypergraphFromInputFile("test_instances/simple_pipeline.hgr", 2,
                                                        "test_instances/simple_pipeline.lvl");

  ASSERT_TRUE(hypergraph.hasTopologicalLevels());
  ASSERT_THAT(hypergraph.initialNumNodes(), Eq(4));
  ASSERT_THAT(hypergraph.initialNumEdges(), Eq(3));
  ASSERT_THAT(hypergraph.topologicalLevel(0), Eq(0));
  ASSERT_THAT(hypergraph.topologicalLevel(1), Eq(1));
  ASSERT_THAT(hypergraph.topologicalLevel(2), Eq(2));
  ASSERT_THAT(hypergraph.topologicalLevel(3), Eq(3));
}

}  // namespace io
}  // namespace kahypar
