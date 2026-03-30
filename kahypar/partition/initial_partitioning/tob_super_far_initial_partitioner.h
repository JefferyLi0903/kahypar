/*******************************************************************************
 * This file is part of KaHyPar.
 *
 * Copyright (C) 2026
 *
 * KaHyPar is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 ******************************************************************************/

#pragma once

#include <algorithm>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

#include "kahypar/definitions.h"
#include "kahypar/partition/initial_partitioning/i_initial_partitioner.h"
#include "kahypar/partition/initial_partitioning/initial_partitioner_base.h"
#include "kahypar/partition/tob_utils.h"
#include "kahypar-resources/utils/randomize.h"

namespace kahypar {

class TOBSuperFarInitialPartitioner final :
  public IInitialPartitioner,
  private InitialPartitionerBase<TOBSuperFarInitialPartitioner> {
 private:
  using Base = InitialPartitionerBase<TOBSuperFarInitialPartitioner>;
  friend Base;

  enum class GrowStrategy : uint8_t {
    global,
    sequential,
    round_robin
  };

 public:
  TOBSuperFarInitialPartitioner(Hypergraph& hypergraph, Context& context) :
    Base(hypergraph, context) { }

  ~TOBSuperFarInitialPartitioner() override = default;

  TOBSuperFarInitialPartitioner(const TOBSuperFarInitialPartitioner&) = delete;
  TOBSuperFarInitialPartitioner& operator= (const TOBSuperFarInitialPartitioner&) = delete;
  TOBSuperFarInitialPartitioner(TOBSuperFarInitialPartitioner&&) = delete;
  TOBSuperFarInitialPartitioner& operator= (TOBSuperFarInitialPartitioner&&) = delete;

 private:
  void partitionImpl() override final {
    Base::multipleRunsInitialPartitioning();
  }

  void initialPartition() {
    const PartitionID old_unassigned = _context.initial_partitioning.unassigned_part;
    _context.initial_partitioning.unassigned_part = -1;
    Base::resetPartitioning();

    std::vector<HypernodeID> roots = selectSuperFarRoots();
    std::vector<PartitionID> best_partition(_hg.initialNumNodes(), Hypergraph::kInvalidPartition);
    HyperedgeWeight best_tob = std::numeric_limits<HyperedgeWeight>::max();
    HyperedgeWeight best_cut = std::numeric_limits<HyperedgeWeight>::max();
    double best_imbalance = std::numeric_limits<double>::max();

    for (const GrowStrategy strategy : { GrowStrategy::global, GrowStrategy::sequential, GrowStrategy::round_robin }) {
      Base::resetPartitioning();
      assignRoots(roots);
      runGrowStrategy(strategy);

      const HyperedgeWeight tob = metrics::topologyDifference(_hg);
      const HyperedgeWeight cut = metrics::hyperedgeCut(_hg);
      const double imbalance = metrics::imbalance(_hg, _context);
      if (metrics::isBetterPartition(tob, best_tob, cut, best_cut,
                                     imbalance, best_imbalance, _context.partition.epsilon)) {
        best_tob = tob;
        best_cut = cut;
        best_imbalance = imbalance;
        for (const HypernodeID& hn : _hg.nodes()) {
          best_partition[hn] = _hg.partID(hn);
        }
      }
    }

    Base::resetPartitioning();
    for (const HypernodeID& hn : _hg.nodes()) {
      if (best_partition[hn] != Hypergraph::kInvalidPartition) {
        _hg.setNodePart(hn, best_partition[hn]);
      }
    }
    assignRemainingNodesFallback();
    _hg.initializeNumCutHyperedges();
    _context.initial_partitioning.unassigned_part = old_unassigned;
    Base::performFMRefinement();
  }

  std::vector<HypernodeID> selectSuperFarRoots() const {
    std::vector<HypernodeID> roots;
    roots.reserve(_context.initial_partitioning.k);

    HypernodeID first = kInvalidNode;
    HypernodeWeight max_weight = 0;
    for (const HypernodeID& hn : _hg.nodes()) {
      if (_hg.isFixedVertex(hn)) {
        continue;
      }
      if (first == kInvalidNode || _hg.nodeWeight(hn) > max_weight) {
        first = hn;
        max_weight = _hg.nodeWeight(hn);
      }
    }
    if (first == kInvalidNode) {
      return roots;
    }
    roots.push_back(first);

    while (roots.size() < _context.initial_partitioning.k) {
      std::vector<int> dist = multiSourceDistance(roots);
      HypernodeID farthest = kInvalidNode;
      int max_dist = -1;
      for (const HypernodeID& hn : _hg.nodes()) {
        if (_hg.isFixedVertex(hn) || std::find(roots.begin(), roots.end(), hn) != roots.end()) {
          continue;
        }
        if (dist[hn] > max_dist) {
          max_dist = dist[hn];
          farthest = hn;
        }
      }
      if (farthest == kInvalidNode) {
        break;
      }
      roots.push_back(farthest);
    }
    return roots;
  }

  std::vector<int> multiSourceDistance(const std::vector<HypernodeID>& sources) const {
    std::vector<int> dist(_hg.initialNumNodes(), std::numeric_limits<int>::max());
    std::queue<HypernodeID> q;
    for (const HypernodeID& s : sources) {
      dist[s] = 0;
      q.push(s);
    }
    while (!q.empty()) {
      const HypernodeID u = q.front();
      q.pop();
      for (const HyperedgeID& he : _hg.incidentEdges(u)) {
        for (const HypernodeID& v : _hg.pins(he)) {
          if (dist[v] > dist[u] + 1) {
            dist[v] = dist[u] + 1;
            q.push(v);
          }
        }
      }
    }
    return dist;
  }

  void assignRoots(const std::vector<HypernodeID>& roots) {
    PartitionID part = 0;
    for (const HypernodeID& root : roots) {
      if (part >= _context.initial_partitioning.k) {
        break;
      }
      if (_hg.partID(root) == Hypergraph::kInvalidPartition) {
        Base::assignHypernodeToPartition(root, part);
      }
      ++part;
    }
  }

  void runGrowStrategy(const GrowStrategy strategy) {
    TOBMetricState<Hypergraph> state(_hg);
    PartitionID rr_part = 0;
    while (true) {
      HypernodeID best_hn = kInvalidNode;
      PartitionID best_part = Hypergraph::kInvalidPartition;
      double best_gain = std::numeric_limits<double>::lowest();

      if (strategy == GrowStrategy::global) {
        selectBestGlobal(state, best_hn, best_part, best_gain);
      } else if (strategy == GrowStrategy::sequential) {
        selectBestSequential(state, best_hn, best_part, best_gain);
      } else {
        selectBestRoundRobin(state, rr_part, best_hn, best_part, best_gain);
      }

      if (best_hn == kInvalidNode || best_part == Hypergraph::kInvalidPartition) {
        break;
      }
      Base::assignHypernodeToPartition(best_hn, best_part);
      state.applyAssignment(_hg.topologicalLevel(best_hn), best_part);
    }
  }

  void selectBestGlobal(const TOBMetricState<Hypergraph>& state,
                        HypernodeID& best_hn,
                        PartitionID& best_part,
                        double& best_gain) const {
    for (const HypernodeID& hn : _hg.nodes()) {
      if (_hg.partID(hn) != Hypergraph::kInvalidPartition || _hg.isFixedVertex(hn)) {
        continue;
      }
      for (PartitionID part = 0; part < _context.initial_partitioning.k; ++part) {
        if (!isFeasibleAssign(hn, part)) {
          continue;
        }
        const double gain = assignmentGain(hn, part, state);
        if (gain > best_gain) {
          best_gain = gain;
          best_hn = hn;
          best_part = part;
        }
      }
    }
  }

  void selectBestSequential(const TOBMetricState<Hypergraph>& state,
                            HypernodeID& best_hn,
                            PartitionID& best_part,
                            double& best_gain) const {
    for (PartitionID part = 0; part < _context.initial_partitioning.k; ++part) {
      if (_hg.partWeight(part) >= _context.initial_partitioning.upper_allowed_partition_weight[part]) {
        continue;
      }
      for (const HypernodeID& hn : _hg.nodes()) {
        if (_hg.partID(hn) != Hypergraph::kInvalidPartition || _hg.isFixedVertex(hn)) {
          continue;
        }
        if (!isFeasibleAssign(hn, part)) {
          continue;
        }
        const double gain = assignmentGain(hn, part, state);
        if (gain > best_gain) {
          best_gain = gain;
          best_hn = hn;
          best_part = part;
        }
      }
      if (best_hn != kInvalidNode) {
        return;
      }
    }
  }

  void selectBestRoundRobin(const TOBMetricState<Hypergraph>& state,
                            PartitionID& rr_part,
                            HypernodeID& best_hn,
                            PartitionID& best_part,
                            double& best_gain) const {
    for (PartitionID offset = 0; offset < _context.initial_partitioning.k; ++offset) {
      const PartitionID part = (rr_part + offset) % _context.initial_partitioning.k;
      if (_hg.partWeight(part) >= _context.initial_partitioning.upper_allowed_partition_weight[part]) {
        continue;
      }
      for (const HypernodeID& hn : _hg.nodes()) {
        if (_hg.partID(hn) != Hypergraph::kInvalidPartition || _hg.isFixedVertex(hn)) {
          continue;
        }
        if (!isFeasibleAssign(hn, part)) {
          continue;
        }
        const double gain = assignmentGain(hn, part, state);
        if (gain > best_gain) {
          best_gain = gain;
          best_hn = hn;
          best_part = part;
        }
      }
      if (best_hn != kInvalidNode) {
        rr_part = (part + 1) % _context.initial_partitioning.k;
        return;
      }
    }
  }

  bool isFeasibleAssign(const HypernodeID hn, const PartitionID part) const {
    return _hg.partWeight(part) + _hg.nodeWeight(hn) <=
           _context.initial_partitioning.upper_allowed_partition_weight[part];
  }

  double assignmentGain(const HypernodeID hn,
                        const PartitionID part,
                        const TOBMetricState<Hypergraph>& state) const {
    const int32_t tau = _hg.topologicalLevel(hn);
    double conn_gain = 0.0;
    for (const HyperedgeID& he : _hg.incidentEdges(hn)) {
      if (_hg.pinCountInPart(he, part) > 0) {
        conn_gain += _hg.edgeWeight(he);
      }
    }
    return _context.partition.tob_delta * state.assignGain(tau, part) + conn_gain;
  }

  void assignRemainingNodesFallback() {
    for (const HypernodeID& hn : _hg.nodes()) {
      if (_hg.partID(hn) != Hypergraph::kInvalidPartition || _hg.isFixedVertex(hn)) {
        continue;
      }
      PartitionID best_part = 0;
      HypernodeWeight best_weight = _hg.partWeight(0);
      for (PartitionID part = 1; part < _context.initial_partitioning.k; ++part) {
        if (_hg.partWeight(part) < best_weight) {
          best_weight = _hg.partWeight(part);
          best_part = part;
        }
      }
      _hg.setNodePart(hn, best_part);
    }
  }

  using Base::_context;
  using Base::_hg;
  static constexpr HypernodeID kInvalidNode = std::numeric_limits<HypernodeID>::max();
};

}  // namespace kahypar
