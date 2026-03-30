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
#include <cmath>
#include <cstdint>
#include <vector>

namespace kahypar {

template <typename HypergraphT>
class TOBMetricState {
 public:
  using PartitionID = typename HypergraphT::PartitionID;
  using HypernodeID = typename HypergraphT::HypernodeID;
  using HyperedgeWeight = typename HypergraphT::HyperedgeWeight;

  static constexpr double kScale = 1e9;

  explicit TOBMetricState(const HypergraphT& hg) :
    _k(hg.k()),
    _tau_max(maxTopologicalLevel(hg)),
    _counts(static_cast<size_t>(_k), std::vector<int>(static_cast<size_t>(_tau_max) + 1, 0)),
    _totals(static_cast<size_t>(_tau_max) + 1, 0) {
    if (!hg.hasTopologicalLevels()) {
      _tau_max = 0;
      _counts.assign(static_cast<size_t>(_k), std::vector<int>(1, 0));
      _totals.assign(1, 0);
      return;
    }

    for (const HypernodeID& hn : hg.nodes()) {
      const int32_t tau = hg.topologicalLevel(hn);
      if (tau < 0) {
        continue;
      }
      ++_totals[static_cast<size_t>(tau)];
      const PartitionID part = hg.partID(hn);
      if (part == HypergraphT::kInvalidPartition) {
        continue;
      }
      ++_counts[part][static_cast<size_t>(tau)];
    }
  }

  static int32_t maxTopologicalLevel(const HypergraphT& hg) {
    if (!hg.hasTopologicalLevels()) {
      return 0;
    }
    int32_t tau_max = 0;
    for (const HypernodeID& hn : hg.nodes()) {
      tau_max = std::max(tau_max, hg.topologicalLevel(hn));
    }
    return tau_max;
  }

  double metric() const {
    double metric_val = 0.0;
    for (int32_t tau = 0; tau <= _tau_max; ++tau) {
      const int total = _totals[static_cast<size_t>(tau)];
      if (total <= 0) {
        continue;
      }
      const double avg = static_cast<double>(total) / _k;
      double sq_sum = 0.0;
      for (PartitionID part = 0; part < _k; ++part) {
        const double diff = static_cast<double>(_counts[part][static_cast<size_t>(tau)]) - avg;
        sq_sum += diff * diff;
      }
      metric_val += sq_sum / (_k * static_cast<double>(total) * static_cast<double>(total));
    }
    return metric_val;
  }

  HyperedgeWeight scaledMetric() const {
    return static_cast<HyperedgeWeight>(std::llround(metric() * kScale));
  }

  double moveGain(const int32_t tau,
                  const PartitionID from_part,
                  const PartitionID to_part) const {
    if (from_part == to_part || tau < 0 || tau > _tau_max) {
      return 0.0;
    }
    const int total = _totals[static_cast<size_t>(tau)];
    if (total <= 0) {
      return 0.0;
    }
    const int from_count = _counts[from_part][static_cast<size_t>(tau)];
    const int to_count = _counts[to_part][static_cast<size_t>(tau)];
    const double denom = static_cast<double>(_k) *
                         static_cast<double>(total) *
                         static_cast<double>(total);
    return (2.0 * (static_cast<double>(from_count - to_count - 1))) / denom;
  }

  HyperedgeWeight scaledMoveGain(const int32_t tau,
                                 const PartitionID from_part,
                                 const PartitionID to_part) const {
    return static_cast<HyperedgeWeight>(std::llround(moveGain(tau, from_part, to_part) * kScale));
  }

  double assignGain(const int32_t tau, const PartitionID to_part) const {
    if (tau < 0 || tau > _tau_max) {
      return 0.0;
    }
    const int total = _totals[static_cast<size_t>(tau)];
    if (total <= 0) {
      return 0.0;
    }
    const double avg = static_cast<double>(total) / _k;
    const double to_count = static_cast<double>(_counts[to_part][static_cast<size_t>(tau)]);
    const double denom = static_cast<double>(total) * static_cast<double>(total);
    return (((1.0 - static_cast<double>(_k)) / _k) - 2.0 * (to_count - avg)) / denom;
  }

  void applyMove(const int32_t tau,
                 const PartitionID from_part,
                 const PartitionID to_part) {
    if (from_part == to_part || tau < 0 || tau > _tau_max) {
      return;
    }
    --_counts[from_part][static_cast<size_t>(tau)];
    ++_counts[to_part][static_cast<size_t>(tau)];
  }

  void applyAssignment(const int32_t tau, const PartitionID to_part) {
    if (tau < 0 || tau > _tau_max) {
      return;
    }
    ++_counts[to_part][static_cast<size_t>(tau)];
  }

 private:
  PartitionID _k;
  int32_t _tau_max;
  std::vector<std::vector<int>> _counts;
  std::vector<int> _totals;
};

}  // namespace kahypar
