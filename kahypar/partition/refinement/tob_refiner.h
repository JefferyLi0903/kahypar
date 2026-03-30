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

#include <array>
#include <limits>
#include <vector>

#include "kahypar/definitions.h"
#include "kahypar/partition/context.h"
#include "kahypar/partition/metrics.h"
#include "kahypar/partition/refinement/i_refiner.h"
#include "kahypar/partition/tob_utils.h"
#include "kahypar-resources/utils/randomize.h"

namespace kahypar {

class TOBRefiner final : public IRefiner {
 private:
  struct MoveCandidate {
    HypernodeID hn = std::numeric_limits<HypernodeID>::max();
    PartitionID from = Hypergraph::kInvalidPartition;
    PartitionID to = Hypergraph::kInvalidPartition;
    double score = std::numeric_limits<double>::lowest();
    double tob_gain = 0.0;
    Gain cut_gain = 0;
    bool valid = false;
  };

 public:
  TOBRefiner(Hypergraph& hypergraph, const Context& context) :
    _hg(hypergraph),
    _context(context) { }

  ~TOBRefiner() override = default;

  TOBRefiner(const TOBRefiner&) = delete;
  TOBRefiner& operator= (const TOBRefiner&) = delete;
  TOBRefiner(TOBRefiner&&) = delete;
  TOBRefiner& operator= (TOBRefiner&&) = delete;

 private:
  void initializeImpl(const HyperedgeWeight) override final {
    _is_initialized = true;
  }

  bool refineImpl(std::vector<HypernodeID>& refinement_nodes,
                  const std::array<HypernodeWeight, 2>&,
                  const UncontractionGainChanges&,
                  Metrics& best_metrics) override final {
    if (_context.partition.objective != Objective::tob || !_hg.hasTopologicalLevels()) {
      return false;
    }

    TOBMetricState<Hypergraph> state(_hg);
    HyperedgeWeight current_tob = state.scaledMetric();
    HyperedgeWeight current_cut = metrics::hyperedgeCut(_hg);
    double current_imbalance = metrics::imbalance(_hg, _context);

    std::vector<PartitionID> best_partition(_hg.initialNumNodes(), 0);
    for (const HypernodeID& hn : _hg.nodes()) {
      best_partition[hn] = _hg.partID(hn);
    }
    HyperedgeWeight best_tob = current_tob;
    HyperedgeWeight best_cut = current_cut;
    double best_imbalance = current_imbalance;

    bool moved_any = false;
    constexpr int kMaxRounds = 4;
    for (int round = 0; round < kMaxRounds; ++round) {
      bool improved_round = false;

      Randomize::instance().shuffleVector(refinement_nodes, refinement_nodes.size());
      std::vector<bool> locked(_hg.initialNumNodes(), false);

      // Phase 1: boundary refinement
      for (const HypernodeID& hn : refinement_nodes) {
        if (locked[hn] || _hg.isFixedVertex(hn) || !_hg.isBorderNode(hn)) {
          continue;
        }
        MoveCandidate cand = bestMoveForNode(hn, state, true);
        if (cand.valid && cand.score > 0.0 && moveIsFeasible(cand.hn, cand.from, cand.to)) {
          applyMove(cand, state, current_tob, current_cut, current_imbalance);
          locked[hn] = true;
          improved_round = true;
          moved_any = true;
          considerAsBest(current_tob, current_cut, current_imbalance,
                         best_tob, best_cut, best_imbalance, best_partition);
        }
      }

      // Phase 2: local refinement
      for (const HypernodeID& hn : refinement_nodes) {
        if (_hg.isFixedVertex(hn)) {
          continue;
        }
        MoveCandidate cand = bestMoveForNode(hn, state, false);
        if (cand.valid && cand.score > 0.0 && moveIsFeasible(cand.hn, cand.from, cand.to)) {
          applyMove(cand, state, current_tob, current_cut, current_imbalance);
          improved_round = true;
          moved_any = true;
          considerAsBest(current_tob, current_cut, current_imbalance,
                         best_tob, best_cut, best_imbalance, best_partition);
        }
      }

      if (!improved_round) {
        break;
      }
    }

    restoreBestPartition(best_partition);
    best_metrics.tob = metrics::topologyDifference(_hg);
    best_metrics.cut = metrics::hyperedgeCut(_hg);
    best_metrics.km1 = metrics::km1(_hg);
    best_metrics.imbalance = metrics::imbalance(_hg, _context);
    return moved_any;
  }

  MoveCandidate bestMoveForNode(const HypernodeID hn,
                                const TOBMetricState<Hypergraph>& state,
                                const bool boundary_only) const {
    MoveCandidate best;
    const PartitionID from = _hg.partID(hn);
    if (from == Hypergraph::kInvalidPartition) {
      return best;
    }
    if (boundary_only && !_hg.isBorderNode(hn)) {
      return best;
    }

    std::vector<bool> candidate_part(_context.partition.k, false);
    for (const HyperedgeID& he : _hg.incidentEdges(hn)) {
      for (const PartitionID& part : _hg.connectivitySet(he)) {
        if (part != from) {
          candidate_part[part] = true;
        }
      }
    }

    const int32_t tau = _hg.topologicalLevel(hn);
    for (PartitionID to = 0; to < _context.partition.k; ++to) {
      if (!candidate_part[to] || to == from) {
        continue;
      }
      if (!moveIsFeasible(hn, from, to)) {
        continue;
      }
      const double tob_gain = state.moveGain(tau, from, to);
      const Gain cut_gain = cutGain(hn, to);
      const double score = _context.partition.tob_zeta * tob_gain + static_cast<double>(cut_gain);
      if (!best.valid || score > best.score) {
        best.hn = hn;
        best.from = from;
        best.to = to;
        best.score = score;
        best.tob_gain = tob_gain;
        best.cut_gain = cut_gain;
        best.valid = true;
      }
    }
    return best;
  }

  bool moveIsFeasible(const HypernodeID hn, const PartitionID from, const PartitionID to) const {
    if (from == to || _hg.isFixedVertex(hn)) {
      return false;
    }
    return _hg.partWeight(to) + _hg.nodeWeight(hn) <= _context.partition.max_part_weights[to];
  }

  Gain cutGain(const HypernodeID hn, const PartitionID target_part) const {
    const PartitionID source_part = _hg.partID(hn);
    Gain gain = 0;
    for (const HyperedgeID& he : _hg.incidentEdges(hn)) {
      if (_hg.connectivity(he) == 1) {
        gain -= _hg.edgeWeight(he);
      } else {
        const HypernodeID pins_in_source = _hg.pinCountInPart(he, source_part);
        const HypernodeID pins_in_target = _hg.pinCountInPart(he, target_part);
        if (pins_in_source == 1 && pins_in_target == _hg.edgeSize(he) - 1) {
          gain += _hg.edgeWeight(he);
        }
      }
    }
    return gain;
  }

  void applyMove(const MoveCandidate& move,
                 TOBMetricState<Hypergraph>& state,
                 HyperedgeWeight& current_tob,
                 HyperedgeWeight& current_cut,
                 double& current_imbalance) {
    _hg.changeNodePart(move.hn, move.from, move.to);
    state.applyMove(_hg.topologicalLevel(move.hn), move.from, move.to);
    current_tob = state.scaledMetric();
    current_cut = metrics::hyperedgeCut(_hg);
    current_imbalance = metrics::imbalance(_hg, _context);
  }

  void considerAsBest(const HyperedgeWeight current_tob,
                      const HyperedgeWeight current_cut,
                      const double current_imbalance,
                      HyperedgeWeight& best_tob,
                      HyperedgeWeight& best_cut,
                      double& best_imbalance,
                      std::vector<PartitionID>& best_partition) const {
    if (metrics::isBetterPartition(current_tob, best_tob,
                                   current_cut, best_cut,
                                   current_imbalance, best_imbalance,
                                   _context.partition.epsilon)) {
      best_tob = current_tob;
      best_cut = current_cut;
      best_imbalance = current_imbalance;
      for (const HypernodeID& hn : _hg.nodes()) {
        best_partition[hn] = _hg.partID(hn);
      }
    }
  }

  void restoreBestPartition(const std::vector<PartitionID>& best_partition) {
    for (const HypernodeID& hn : _hg.nodes()) {
      const PartitionID current = _hg.partID(hn);
      const PartitionID best = best_partition[hn];
      if (current != best) {
        _hg.changeNodePart(hn, current, best);
      }
    }
  }

  Hypergraph& _hg;
  const Context& _context;
};

}  // namespace kahypar
