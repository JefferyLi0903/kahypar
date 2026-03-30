/*******************************************************************************
 * This file is part of KaHyPar.
 *
 * Copyright (C) 2014-2016 Sebastian Schlag <sebastian.schlag@kit.edu>
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

#pragma once

#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdlib>

#include "kahypar/definitions.h"
#include "kahypar/utils/timer.h"
#include "kahypar/utils/validate.h"

namespace kahypar {
namespace io {
using Mapping = std::unordered_map<HypernodeID, HypernodeID>;
using ErrorList = std::vector<validate::InputError>;
using validate::CheckedIStream;

static bool getNextLine(std::ifstream& file, std::string& line, size_t& line_number) {
  bool success = false;
  do {
    success = static_cast<bool>(std::getline(file, line));
    ++line_number;
    // skip any comments
  } while (success && line[0] == '%');
  return success;
}

static inline void readHGRHeader(std::ifstream& file, HyperedgeID& num_hyperedges,
                                 HypernodeID& num_hypernodes, HypergraphType& hypergraph_type,
                                 size_t& line_number) {
  std::string line;
  getNextLine(file, line, line_number);

  CheckedIStream sstream(line, line_number);
  int i = 0;
  if (sstream >> num_hyperedges && sstream >> num_hypernodes &&
      // note: hypergraph type may be omitted, assuming unweighted
      ( sstream.empty() || (sstream >> i && sstream.empty()) )) {
    hypergraph_type = static_cast<HypergraphType>(i);
  } else {
    ERROR("Invalid Header. Expected <num_hyperedges> <num_hypernodes> <type>", line_number);
  }
}

static inline void readHypergraphFile(const std::string& filename, HypernodeID& num_hypernodes,
                                      HyperedgeID& num_hyperedges,
                                      HyperedgeIndexVector& index_vector,
                                      HyperedgeVector& edge_vector,
                                      HyperedgeWeightVector* hyperedge_weights = nullptr,
                                      HypernodeWeightVector* hypernode_weights = nullptr,
                                      std::vector<size_t>* line_number_vector = nullptr) {
  ASSERT(!filename.empty(), "No filename for hypergraph file specified");
  HypergraphType hypergraph_type = HypergraphType::Unweighted;
  std::ifstream file(filename);
  size_t line_number = 0;
  if (file) {
    readHGRHeader(file, num_hyperedges, num_hypernodes, hypergraph_type, line_number);
    if (hypergraph_type != HypergraphType::Unweighted &&
        hypergraph_type != HypergraphType::EdgeWeights &&
        hypergraph_type != HypergraphType::NodeWeights &&
        hypergraph_type != HypergraphType::EdgeAndNodeWeights) {
      ERROR("Invalid hypergraph type", line_number);
    }

    const bool has_hyperedge_weights = hypergraph_type == HypergraphType::EdgeWeights ||
                                       hypergraph_type == HypergraphType::EdgeAndNodeWeights ?
                                       true : false;
    const bool has_hypernode_weights = hypergraph_type == HypergraphType::NodeWeights ||
                                       hypergraph_type == HypergraphType::EdgeAndNodeWeights ?
                                       true : false;

    index_vector.reserve(static_cast<size_t>(num_hyperedges) +  /*sentinel*/ 1);
    index_vector.push_back(edge_vector.size());
    if (line_number_vector != nullptr) {
      line_number_vector->reserve(static_cast<size_t>(num_hyperedges) +
                                  (has_hypernode_weights ? static_cast<size_t>(num_hypernodes) : 0));
      line_number_vector->clear();
    }

    std::string line;
    for (HyperedgeID i = 0; i < num_hyperedges; ++i) {
      getNextLine(file, line, line_number);
      CheckedIStream line_stream(line, line_number);
      if (line_stream.empty()) {
        ERROR("Hyperedge is empty", line_number);
      }

      if (has_hyperedge_weights) {
        HyperedgeWeight edge_weight;
        line_stream >> edge_weight;
        if (hyperedge_weights == nullptr) {
          LOG << "****** ignoring hyperedge weights ******";
        } else {
          ASSERT(hyperedge_weights != nullptr, "Hypergraph has hyperedge weights");
          hyperedge_weights->push_back(edge_weight);
        }
      }
      HypernodeID pin;
      while (line_stream >> pin) {
        if (pin == 0) {
          ERROR("Invalid index 0 for pin. Vertex indices start with 1", line_number);
        }
        // Hypernode IDs start from 0
        --pin;
        edge_vector.push_back(pin);
      }
      index_vector.push_back(edge_vector.size());
      if (line_number_vector != nullptr) {
        line_number_vector->push_back(line_number);
      }
    }

    if (has_hypernode_weights) {
      if (hypernode_weights == nullptr) {
        LOG << " ****** ignoring hypernode weights ******";
      } else {
        ASSERT(hypernode_weights != nullptr, "Hypergraph has hypernode weights");
        for (HypernodeID i = 0; i < num_hypernodes; ++i) {
          getNextLine(file, line, line_number);
          CheckedIStream line_stream(line, line_number);
          HypernodeWeight node_weight;
          if (line_stream >> node_weight) {
            hypernode_weights->push_back(node_weight);
          } else {
            ERROR("Hypergraph has " << num_hypernodes << " hypernodes, but found only " << i << " weights");
          }
          if (!line_stream.empty()) {
            ERROR("Expected hypernode weight, but line contains multiple entries", line_number);
          }
          if (line_number_vector != nullptr) {
            line_number_vector->push_back(line_number);
          }
        }
      }
    }

    if (getNextLine(file, line, line_number) && !CheckedIStream(line).empty()) {
      WARNING("Unexpected content after end of hypergraph data", line_number);
    }
    file.close();
  } else {
    ERROR("File not found.");
  }
}

static inline void readHypergraphFile(const std::string& filename,
                                      HypernodeID& num_hypernodes,
                                      HyperedgeID& num_hyperedges,
                                      std::unique_ptr<size_t[]>& index_vector,
                                      std::unique_ptr<HypernodeID[]>& edge_vector,
                                      std::unique_ptr<HyperedgeWeight[]>& hyperedge_weights,
                                      std::unique_ptr<HypernodeWeight[]>& hypernode_weights) {
  HyperedgeIndexVector index_vec;
  HyperedgeVector edge_vec;
  HyperedgeWeightVector edge_weights_vec;
  HypernodeWeightVector node_weights_vec;

  readHypergraphFile(filename, num_hypernodes, num_hyperedges, index_vec,
                     edge_vec, &edge_weights_vec, &node_weights_vec);

  ASSERT(index_vector == nullptr);
  ASSERT(edge_vector == nullptr);
  index_vector = std::make_unique<size_t[]>(index_vec.size());
  edge_vector = std::make_unique<HypernodeID[]>(edge_vec.size());

  memcpy(index_vector.get(), index_vec.data(), index_vec.size() * sizeof(size_t));
  memcpy(edge_vector.get(), edge_vec.data(), edge_vec.size() * sizeof(HypernodeID));

  if (!edge_weights_vec.empty()) {
    ASSERT(hyperedge_weights == nullptr);
    hyperedge_weights = std::make_unique<HyperedgeWeight[]>(edge_weights_vec.size());
    memcpy(hyperedge_weights.get(), edge_weights_vec.data(),
           edge_weights_vec.size() * sizeof(HyperedgeWeight));
  }

  if (!node_weights_vec.empty()) {
    ASSERT(hypernode_weights == nullptr);
    hypernode_weights = std::make_unique<HypernodeWeight[]>(node_weights_vec.size());
    memcpy(hypernode_weights.get(), node_weights_vec.data(),
           node_weights_vec.size() * sizeof(HypernodeWeight));
  }
}

static inline void validateAndPrintErrors(const HypernodeID num_hypernodes, const HyperedgeID num_hyperedges,
                                          const size_t* hyperedge_indices, const HypernodeID* hyperedges,
                                          const HyperedgeWeight* hyperedge_weights, const HypernodeWeight* vertex_weights,
                                          const std::vector<size_t> line_numbers,
                                          std::vector<HyperedgeID>& ignored_hes,
                                          std::vector<size_t>& ignored_pins,
                                          const bool promote_warnings_to_errors) {
    ErrorList errors;
    HighResClockTimepoint start = std::chrono::high_resolution_clock::now();
    validate::validateInput(num_hypernodes, num_hyperedges, hyperedge_indices, hyperedges,
                            hyperedge_weights, vertex_weights, &errors, &ignored_hes, &ignored_pins);
    validate::printErrors(num_hyperedges, errors, line_numbers, promote_warnings_to_errors);
    if (validate::containsFatalError(errors, promote_warnings_to_errors)) {
      exit(1);
    }
    HighResClockTimepoint end = std::chrono::high_resolution_clock::now();
    Timer::instance().add(Timepoint::input_validation,
                          std::chrono::duration<double>(end - start).count());
}

static inline Hypergraph createHypergraphFromFile(const std::string& filename,
                                                  const PartitionID num_parts,
                                                  bool validate_input,
                                                  bool promote_warnings_to_errors) {
  HypernodeID num_hypernodes;
  HyperedgeID num_hyperedges;
  HyperedgeIndexVector index_vector;
  HyperedgeVector edge_vector;
  HypernodeWeightVector hypernode_weights;
  HyperedgeWeightVector hyperedge_weights;
  std::vector<size_t> line_numbers;
  readHypergraphFile(filename, num_hypernodes, num_hyperedges,
                     index_vector, edge_vector, &hyperedge_weights, &hypernode_weights,
                     validate_input ? &line_numbers : nullptr);

  if (validate_input) {
    std::vector<HyperedgeID> ignored_hes;
    std::vector<size_t> ignored_pins;
    validateAndPrintErrors(num_hypernodes, num_hyperedges, index_vector.data(), edge_vector.data(),
                           hyperedge_weights.empty() ? nullptr : hyperedge_weights.data(),
                           hypernode_weights.empty() ? nullptr : hypernode_weights.data(),
                           line_numbers, ignored_hes, ignored_pins, promote_warnings_to_errors);
    return Hypergraph(num_hypernodes, num_hyperedges, index_vector, edge_vector,
                      num_parts, &hyperedge_weights, &hypernode_weights, ignored_hes, ignored_pins);
  }

  return Hypergraph(num_hypernodes, num_hyperedges, index_vector, edge_vector,
                    num_parts, &hyperedge_weights, &hypernode_weights);
}

static inline bool hasFileExtension(const std::string& filename, const std::string& extension) {
  return filename.size() >= extension.size() &&
         filename.compare(filename.size() - extension.size(), extension.size(), extension) == 0;
}


static inline void writeHypernodeWeights(std::ofstream& out_stream, const Hypergraph& hypergraph) {
  for (const HypernodeID& hn : hypergraph.nodes()) {
    out_stream << hypergraph.nodeWeight(hn) << std::endl;
  }
}

static inline void writeHGRHeader(std::ofstream& out_stream, const Hypergraph& hypergraph) {
  out_stream << hypergraph.initialNumEdges() << " " << hypergraph.initialNumNodes() << " ";
  if (hypergraph.type() != HypergraphType::Unweighted) {
    out_stream << static_cast<int>(hypergraph.type());
  }
  out_stream << std::endl;
}

static inline void writeHypergraphFile(const Hypergraph& hypergraph, const std::string& filename) {
  ASSERT(!filename.empty(), "No filename for hypergraph file specified");
  ALWAYS_ASSERT(!hypergraph.isModified(), "Hypergraph is modified. Reindexing HNs/HEs necessary.");

  std::ofstream out_stream(filename.c_str());
  writeHGRHeader(out_stream, hypergraph);

  for (const HyperedgeID& he : hypergraph.edges()) {
    if (hypergraph.type() == HypergraphType::EdgeWeights ||
        hypergraph.type() == HypergraphType::EdgeAndNodeWeights) {
      out_stream << hypergraph.edgeWeight(he) << " ";
    }
    for (const HypernodeID& pin : hypergraph.pins(he)) {
      out_stream << pin + 1 << " ";
    }
    out_stream << std::endl;
  }

  if (hypergraph.type() == HypergraphType::NodeWeights ||
      hypergraph.type() == HypergraphType::EdgeAndNodeWeights) {
    writeHypernodeWeights(out_stream, hypergraph);
  }
  out_stream.close();
}


static inline void writeHypergraphToGraphMLFile(const Hypergraph& hypergraph,
                                                const std::string& filename,
                                                const std::vector<PartitionID>* hn_cluster_ids = nullptr,
                                                const std::vector<PartitionID>* he_cluster_ids = nullptr) {
  std::ofstream out_stream(filename.c_str());

  out_stream << R"(<?xml version="1.0" encoding="UTF-8" standalone="no"?>)"
             << R"( <graphml xmlns="http://graphml.graphdrawing.org/xmlns")"
             << R"( xmlns:java="http://www.yworks.com/xml/yfiles-common/1.0/java")"
             << R"( xmlns:sys="http://www.yworks.com/xml/yfiles-common/markup/primitives/2.0")"
             << R"( xmlns:x="http://www.yworks.com/xml/yfiles-common/markup/2.0")"
             << R"( xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance")"
             << R"( xmlns:y="http://www.yworks.com/xml/graphml")"
             << R"( xmlns:yed="http://www.yworks.com/xml/yed/3")"
             << R"( xsi:schemaLocation="http://graphml.graphdrawing.org/xmlns)"
             << R"(http://www.yworks.com/xml/schema/graphml/1.1/ygraphml.xsd">)"
             << std::endl;

  out_stream << R"(<key id="d0" for="node" attr.name="weight" attr.type="double"/>)" << std::endl;
  out_stream << R"(<key id="d1" for="node" attr.name="part" attr.type="int"/>)" << std::endl;
  out_stream << R"(<key id="d2" for="node" attr.name="iscutedge" attr.type="int"/>)" << std::endl;
  out_stream << R"(<key id="d7" for="node" attr.name="modclass" attr.type="int"/>)" << std::endl;
  out_stream << R"(<key id="d8" for="node" attr.name="color" attr.type="string"/>)" << std::endl;
  out_stream << R"(<graph id="G" edgedefault="undirected">)" << std::endl;
  for (const HypernodeID& hn : hypergraph.nodes()) {
    out_stream << R"(<node id="n)" << hn << R"(">)" << std::endl;
    out_stream << R"(<data key="d0">)" << hypergraph.nodeWeight(hn) << "</data>" << std::endl;
    if (hn_cluster_ids != nullptr) {
      out_stream << R"(<data key="d7">)" << (*hn_cluster_ids)[hn] << "</data>" << std::endl;
    } else {
      out_stream << R"(<data key="d1">)" << hypergraph.partID(hn) << "</data>" << std::endl;
    }

    out_stream << R"(<data key="d2">)" << 42 << "</data>" << std::endl;
    out_stream << R"(<data key="d8">)" << "blue" << "</data>" << std::endl;
    out_stream << "</node>" << std::endl;
  }

  HyperedgeID edge_id = 0;
  for (const HyperedgeID& he : hypergraph.edges()) {
    // const HyperedgeID he_id = hypergraph.initialNumNodes() + he;
    out_stream << R"(<node id="h)" << he << R"(">)" << std::endl;
    out_stream << R"(<data key="d0">)" << hypergraph.edgeWeight(he) << "</data>" << std::endl;
    if (he_cluster_ids != nullptr) {
      out_stream << R"(<data key="d7">)" << (*he_cluster_ids)[he] << "</data>" << std::endl;
    } else {
      out_stream << R"(<data key="d1">)" << -1 << "</data>" << std::endl;
    }
    out_stream << R"(<data key="d2">)" << (hypergraph.connectivity(he) > 1) << "</data>" << std::endl;
    out_stream << R"(<data key="d8">)" << "red" << "</data>" << std::endl;
    out_stream << "</node>" << std::endl;
    for (const HypernodeID& pin : hypergraph.pins(he)) {
      out_stream << R"(<edge id="e)" << edge_id++ << R"(" source="n)" << pin << R"(" target="h)"
                 << he << R"("/>)" << std::endl;
    }
  }

  out_stream << "</graph>" << std::endl;
  out_stream << "</graphml>" << std::endl;
  out_stream.close();
}


static inline void writeHypergraphForhMetisPartitioning(const Hypergraph& hypergraph,
                                                        const std::string& filename,
                                                        const Mapping& mapping) {
  ASSERT(!filename.empty(), "No filename for hMetis initial partitioning file specified");
  std::ofstream out_stream(filename.c_str());

  // coarse graphs always have edge and node weights, even if graph wasn't coarsend
  out_stream << hypergraph.currentNumEdges() << " " << hypergraph.currentNumNodes() << " ";
  out_stream << static_cast<int>(HypergraphType::EdgeAndNodeWeights);
  out_stream << std::endl;

  for (const HyperedgeID& he : hypergraph.edges()) {
    out_stream << hypergraph.edgeWeight(he) << " ";
    for (const HypernodeID& pin : hypergraph.pins(he)) {
      ASSERT(mapping.find(pin) != mapping.end(), "No mapping found for pin " << pin);
      out_stream << mapping.find(pin)->second + 1 << " ";
    }
    out_stream << std::endl;
  }

  writeHypernodeWeights(out_stream, hypergraph);
  out_stream.close();
}

static inline void writeHypergraphForPaToHPartitioning(const Hypergraph& hypergraph,
                                                       const std::string& filename,
                                                       const Mapping& mapping) {
  ASSERT(!filename.empty(), "No filename for PaToH initial partitioning file specified");
  std::ofstream out_stream(filename.c_str());
  out_stream << 1;                     // 1-based indexing
  out_stream << " " << hypergraph.currentNumNodes() << " " << hypergraph.currentNumEdges() << " " << hypergraph.currentNumPins();
  out_stream << " " << 3 << std::endl;  // weighting scheme: both edge and node weights

  for (const HyperedgeID& he : hypergraph.edges()) {
    out_stream << hypergraph.edgeWeight(he) << " ";
    for (const HypernodeID& pin : hypergraph.pins(he)) {
      ASSERT(mapping.find(pin) != mapping.end(), "No mapping found for pin " << pin);
      out_stream << mapping.find(pin)->second + 1 << " ";
    }
    out_stream << std::endl;
  }

  for (const HypernodeID& hn : hypergraph.nodes()) {
    out_stream << hypergraph.nodeWeight(hn) << " ";
  }
  out_stream << std::endl;
  out_stream.close();
}

static inline void writeHypergraphForPaToHPartitioning(const Hypergraph& hypergraph,
                                                       const std::string& filename) {
  ASSERT(!filename.empty(), "No filename for PaToH initial partitioning file specified");
  std::ofstream out_stream(filename.c_str());
  out_stream << 0;                     // 0-based indexing
  out_stream << " " << hypergraph.currentNumNodes() << " " << hypergraph.currentNumEdges() << " " << hypergraph.currentNumPins();
  out_stream << " " << 3 << std::endl;  // weighting scheme: both edge and node weights

  for (const HyperedgeID& he : hypergraph.edges()) {
    out_stream << hypergraph.edgeWeight(he) << " ";
    for (const HypernodeID& pin : hypergraph.pins(he)) {
      // ASSERT(mapping.find(pin) != mapping.end(), "No mapping found for pin " << pin);
      out_stream << pin << " ";
    }
    out_stream << "\n";
  }

  for (const HypernodeID& hn : hypergraph.nodes()) {
    out_stream << hypergraph.nodeWeight(hn) << " ";
  }
  out_stream << std::endl;
  out_stream.close();
}


static inline void readPartitionFile(const std::string& filename, std::vector<PartitionID>& partition) {
  ASSERT(!filename.empty(), "No filename for partition file specified");
  ASSERT(partition.empty(), "Partition vector is not empty");
  std::ifstream file(filename);
  if (file) {
    int part;
    while (file >> part) {
      partition.push_back(part);
    }
    file.close();
  } else {
    ERROR("File not found.");
  }
}

static inline void writePartitionFile(const Hypergraph& hypergraph, const std::string& filename) {
  if (!filename.empty()) {
    std::ofstream out_stream(filename.c_str());
    for (const HypernodeID& hn : hypergraph.nodes()) {
      out_stream << hypergraph.partID(hn) << std::endl;
    }
    out_stream.close();
  }
}

static inline void readFixedVertexFile(Hypergraph& hypergraph, const std::string& filename) {
  ASSERT(!filename.empty(), "No filename for partition file specified");
  std::ifstream file(filename);
  if (file) {
    PartitionID part;
    HypernodeID hn = 0;
    while (file >> part) {
      if (part != -1) {
        hypergraph.setFixedVertex(hn, part);
      }
      hn++;
    }
    file.close();
  } else {
    ERROR("File not found: " << filename);
  }
}

static inline void writeFixedVertexFile(const Hypergraph& hypergraph, const std::string& filename) {
  ASSERT(!filename.empty(), "No filename for partition file specified");
  std::ofstream out_stream(filename.c_str());
  for (const HypernodeID& hn : hypergraph.nodes()) {
    out_stream << hypergraph.fixedVertexPartID(hn) << std::endl;
  }
  out_stream.close();
}

// ==================== Topological Level I/O for TOB Objective ====================

/*!
 * Reads topological levels from a file.
 * File format: one level per line, where line i contains the level of hypernode i.
 * Levels are 0-indexed integers.
 *
 * \param hypergraph The hypergraph to set topological levels for
 * \param filename Path to the topological levels file
 */
static inline void readTopologicalLevelFile(Hypergraph& hypergraph, const std::string& filename) {
  ASSERT(!filename.empty(), "No filename for topological level file specified");
  std::ifstream file(filename);
  if (file) {
    int32_t level;
    HypernodeID hn = 0;
    while (file >> level) {
      if (hn < hypergraph.initialNumNodes()) {
        hypergraph.setTopologicalLevel(hn, level);
      }
      ++hn;
    }
    file.close();
    if (hn != hypergraph.initialNumNodes()) {
      LOG << "Warning: Topological level file has " << hn << " entries, but hypergraph has "
          << hypergraph.initialNumNodes() << " nodes.";
    }
  } else {
    ERROR("Topological level file not found: " << filename);
  }
}

/*!
 * Writes topological levels to a file.
 * File format: one level per line, where line i contains the level of hypernode i.
 *
 * \param hypergraph The hypergraph with topological levels
 * \param filename Path to write the topological levels file
 */
static inline void writeTopologicalLevelFile(const Hypergraph& hypergraph, const std::string& filename) {
  ASSERT(!filename.empty(), "No filename for topological level file specified");
  if (!hypergraph.hasTopologicalLevels()) {
    LOG << "Warning: Hypergraph does not have topological levels. Writing empty file.";
  }
  std::ofstream out_stream(filename.c_str());
  for (const HypernodeID& hn : hypergraph.nodes()) {
    if (hypergraph.hasTopologicalLevels()) {
      out_stream << hypergraph.topologicalLevel(hn) << std::endl;
    } else {
      out_stream << 0 << std::endl;
    }
  }
  out_stream.close();
}

/*!
 * Reads a Directed Acyclic Hypergraph (DAH) file and computes topological levels.
 * 
 * DAH file format (extension: .dah):
 * Line 1: <num_hyperedges> <num_hypernodes> [type]
 * Following lines: Each hyperedge is represented as:
 *   [weight] <source_pins...> -> <target_pins...>
 * 
 * The "->" separator distinguishes source pins (tail) from target pins (head).
 * Topological levels are computed such that for each directed hyperedge,
 * all source pins have levels strictly less than all target pins.
 *
 * \param filename Path to the DAH file
 * \param num_hypernodes Output: number of hypernodes
 * \param num_hyperedges Output: number of hyperedges
 * \param index_vector Output: hyperedge index vector
 * \param edge_vector Output: pin vector (all pins, sources and targets combined)
 * \param topological_levels Output: computed topological levels for each node
 * \param hyperedge_weights Optional output: hyperedge weights
 * \param hypernode_weights Optional output: hypernode weights
 */
static inline void readDAHFile(const std::string& filename,
                               HypernodeID& num_hypernodes,
                               HyperedgeID& num_hyperedges,
                               HyperedgeIndexVector& index_vector,
                               HyperedgeVector& edge_vector,
                               std::vector<int32_t>& topological_levels,
                               HyperedgeWeightVector* hyperedge_weights = nullptr,
                               HypernodeWeightVector* hypernode_weights = nullptr) {
  ASSERT(!filename.empty(), "No filename for DAH file specified");
  std::ifstream file(filename);
  size_t line_number = 0;
  
  if (!file) {
    ERROR("DAH file not found: " << filename);
  }
  
  // Read header
  HypergraphType hypergraph_type = HypergraphType::Unweighted;
  readHGRHeader(file, num_hyperedges, num_hypernodes, hypergraph_type, line_number);
  
  const bool has_hyperedge_weights = hypergraph_type == HypergraphType::EdgeWeights ||
                                     hypergraph_type == HypergraphType::EdgeAndNodeWeights;
  const bool has_hypernode_weights = hypergraph_type == HypergraphType::NodeWeights ||
                                     hypergraph_type == HypergraphType::EdgeAndNodeWeights;
  
  // Data structures for DAG representation.
  // We store a deduplicated successor list so Kahn's algorithm remains exact
  // even if multiple hyperedges induce the same precedence relation.
  std::vector<std::unordered_set<HypernodeID>> successors(num_hypernodes);
  std::vector<int32_t> in_degree(num_hypernodes, 0);
  
  index_vector.reserve(static_cast<size_t>(num_hyperedges) + 1);
  index_vector.push_back(0);
  
  std::string line;
  for (HyperedgeID he = 0; he < num_hyperedges; ++he) {
    if (!getNextLine(file, line, line_number)) {
      ERROR("Unexpected end of file at hyperedge " << he);
    }
    
    // Parse the line: [weight] source_pins... -> target_pins...
    std::istringstream line_stream(line);
    
    if (has_hyperedge_weights) {
      HyperedgeWeight weight;
      line_stream >> weight;
      if (hyperedge_weights != nullptr) {
        hyperedge_weights->push_back(weight);
      }
    }
    
    std::vector<HypernodeID> source_pins;
    std::vector<HypernodeID> target_pins;
    bool reading_targets = false;
    std::string token;
    
    while (line_stream >> token) {
      if (token == "->") {
        reading_targets = true;
        continue;
      }
      
      HypernodeID pin = static_cast<HypernodeID>(std::stoul(token));
      if (pin == 0) {
        ERROR("Invalid index 0 for pin. Vertex indices start with 1", line_number);
      }
      --pin;  // Convert to 0-based indexing
      
      if (reading_targets) {
        target_pins.push_back(pin);
      } else {
        source_pins.push_back(pin);
      }
    }
    
    // If no "->" found, treat all pins as undirected (no ordering constraint)
    if (!reading_targets) {
      // All pins go to edge_vector, no ordering constraints
      for (const HypernodeID& pin : source_pins) {
        edge_vector.push_back(pin);
      }
    } else {
      // Add all pins to edge_vector
      for (const HypernodeID& pin : source_pins) {
        edge_vector.push_back(pin);
      }
      for (const HypernodeID& pin : target_pins) {
        edge_vector.push_back(pin);
      }
      
      // Add ordering constraints: all sources must come before all targets
      for (const HypernodeID& target : target_pins) {
        for (const HypernodeID& source : source_pins) {
          if (source != target) {
            if (successors[source].insert(target).second) {
              ++in_degree[target];
            }
          }
        }
      }
    }
    
    index_vector.push_back(edge_vector.size());
  }
  
  // Read hypernode weights if present
  if (has_hypernode_weights && hypernode_weights != nullptr) {
    for (HypernodeID hn = 0; hn < num_hypernodes; ++hn) {
      if (!getNextLine(file, line, line_number)) {
        ERROR("Unexpected end of file while reading hypernode weights");
      }
      std::istringstream line_stream(line);
      HypernodeWeight weight;
      line_stream >> weight;
      hypernode_weights->push_back(weight);
    }
  }
  
  file.close();
  
  // Compute topological levels using Kahn's algorithm
  topological_levels.resize(num_hypernodes, 0);
  std::vector<HypernodeID> queue;
  
  // Initialize queue with nodes having no predecessors
  for (HypernodeID hn = 0; hn < num_hypernodes; ++hn) {
    if (in_degree[hn] == 0) {
      queue.push_back(hn);
      topological_levels[hn] = 0;
    }
  }
  
  size_t processed = 0;
  size_t queue_start = 0;
  
  while (queue_start < queue.size()) {
    HypernodeID current = queue[queue_start++];
    ++processed;
    
    for (const HypernodeID successor : successors[current]) {
      --in_degree[successor];
      // Update level: successor's level must be at least current's level + 1
      topological_levels[successor] = std::max(topological_levels[successor],
                                               topological_levels[current] + 1);
      if (in_degree[successor] == 0) {
        queue.push_back(successor);
      }
    }
  }
  
  if (processed != num_hypernodes) {
    LOG << "Warning: DAH contains a cycle. " << processed << " of " << num_hypernodes
        << " nodes were processed. Remaining nodes assigned level 0.";
  }
}

/*!
 * Creates a hypergraph from a DAH file with topological levels.
 *
 * \param filename Path to the DAH file
 * \param num_parts Number of partitions
 * \return Hypergraph with topological levels set
 */
static inline Hypergraph createHypergraphFromDAHFile(const std::string& filename,
                                                     const PartitionID num_parts) {
  HypernodeID num_hypernodes;
  HyperedgeID num_hyperedges;
  HyperedgeIndexVector index_vector;
  HyperedgeVector edge_vector;
  std::vector<int32_t> topological_levels;
  HyperedgeWeightVector hyperedge_weights;
  HypernodeWeightVector hypernode_weights;
  
  readDAHFile(filename, num_hypernodes, num_hyperedges, index_vector, edge_vector,
              topological_levels, &hyperedge_weights, &hypernode_weights);
  
  Hypergraph hypergraph(num_hypernodes, num_hyperedges, index_vector, edge_vector,
                        num_parts, &hyperedge_weights, &hypernode_weights);
  
  // Set topological levels
  hypergraph.setTopologicalLevels(topological_levels);
  
  return hypergraph;
}

static inline Hypergraph createHypergraphFromInputFile(const std::string& graph_filename,
                                                       const PartitionID num_parts,
                                                       const std::string& topological_level_filename = "",
                                                       const bool validate_input = true,
                                                       const bool promote_warnings_to_errors = true) {
  Hypergraph hypergraph = hasFileExtension(graph_filename, ".dah") ?
    createHypergraphFromDAHFile(graph_filename, num_parts) :
    createHypergraphFromFile(graph_filename, num_parts, validate_input, promote_warnings_to_errors);

  if (!topological_level_filename.empty()) {
    readTopologicalLevelFile(hypergraph, topological_level_filename);
  }

  return hypergraph;
}

/*!
 * Computes topological levels for a hypergraph based on hyperedge structure.
 * This assumes hyperedges represent directed dependencies where the first pin
 * is the "source" and remaining pins are "targets".
 * 
 * For undirected hypergraphs, all nodes get level 0.
 *
 * \param hypergraph The hypergraph to compute levels for
 * \param source_pin_index Index of the source pin in each hyperedge (default: 0)
 */
static inline void computeTopologicalLevelsFromHypergraph(Hypergraph& hypergraph,
                                                          size_t source_pin_index = 0) {
  const HypernodeID num_nodes = hypergraph.initialNumNodes();
  std::vector<int32_t> levels(num_nodes, 0);
  std::vector<int32_t> in_degree(num_nodes, 0);
  std::vector<std::vector<HypernodeID>> successors(num_nodes);
  
  // Build dependency graph from hyperedges
  // Assumption: first pin is source, rest are targets
  for (const HyperedgeID& he : hypergraph.edges()) {
    if (hypergraph.edgeSize(he) <= 1) continue;
    
    std::vector<HypernodeID> pins;
    for (const HypernodeID& pin : hypergraph.pins(he)) {
      pins.push_back(pin);
    }
    
    if (source_pin_index >= pins.size()) continue;
    
    HypernodeID source = pins[source_pin_index];
    for (size_t i = 0; i < pins.size(); ++i) {
      if (i != source_pin_index) {
        HypernodeID target = pins[i];
        if (source != target) {
          successors[source].push_back(target);
          ++in_degree[target];
        }
      }
    }
  }
  
  // Kahn's algorithm for topological sort with level computation
  std::vector<HypernodeID> queue;
  for (HypernodeID hn = 0; hn < num_nodes; ++hn) {
    if (in_degree[hn] == 0) {
      queue.push_back(hn);
    }
  }
  
  size_t queue_start = 0;
  while (queue_start < queue.size()) {
    HypernodeID current = queue[queue_start++];
    
    for (const HypernodeID& successor : successors[current]) {
      levels[successor] = std::max(levels[successor], levels[current] + 1);
      --in_degree[successor];
      if (in_degree[successor] == 0) {
        queue.push_back(successor);
      }
    }
  }
  
  hypergraph.setTopologicalLevels(levels);
}

}  // namespace io
}  // namespace kahypar
