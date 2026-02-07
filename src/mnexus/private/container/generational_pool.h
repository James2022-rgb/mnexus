#pragma once

// c++ headers ------------------------------------------
#include <cassert>
#include <cstdint>

#include <optional>
#include <utility>
#include <vector>

// public project headers -------------------------------
#include "mbase/public/access.h"
#include "mbase/public/assert.h"

namespace container {

struct GenerationalHandle {
  uint32_t index = 0;
  uint32_t generation = 0;

  static constexpr GenerationalHandle Null() { return GenerationalHandle { 0xFFFFFFFFu, 0 }; }

  constexpr static GenerationalHandle FromU64(uint64_t value) {
    return GenerationalHandle {
      .index = static_cast<uint32_t>(value & 0xFFFFFFFFu),
      .generation = static_cast<uint32_t>((value >> 32) & 0xFFFFFFFFu),
    };
  }

  constexpr bool IsNull() const { return index == 0xFFFFFFFFu; }

  constexpr uint64_t AsU64() const {
    return (static_cast<uint64_t>(generation) << 32) | static_cast<uint64_t>(index);
  }

  friend constexpr bool operator==(GenerationalHandle a, GenerationalHandle b) {
    return a.index == b.index && a.generation == b.generation;
  }
  friend constexpr bool operator!=(GenerationalHandle a, GenerationalHandle b) { return !(a == b); }
};

template <class HotT, class ColdT>
class GenerationalPool {
public:
  using Handle = GenerationalHandle;

  using Hot  = HotT;
  using Cold = ColdT;

  GenerationalPool() = default;
  MBASE_DISALLOW_COPY_DEFAULT_MOVE(GenerationalPool);

  void Reserve(uint32_t slot_capacity) {
    hot_.reserve(slot_capacity);
    cold_.reserve(slot_capacity);
    gen_.reserve(slot_capacity);
    freelist_.reserve(slot_capacity);
  }

  uint32_t GetSlotCount() const { return static_cast<uint32_t>(gen_.size()); }

  uint32_t GetLiveCount() const { return live_count_; }

  // In-place construct Hot and Cold.
  template <class... HotArgs, class... ColdArgs>
  Handle Emplace(std::piecewise_construct_t,
                 std::tuple<HotArgs...> hot_args,
                 std::tuple<ColdArgs...> cold_args) {
    uint32_t idx = this->AllocateSlot();

    hot_[idx].emplace(std::make_from_tuple<Hot>(std::move(hot_args)));
    cold_[idx].emplace(std::make_from_tuple<Cold>(std::move(cold_args)));

    ++live_count_;
    return Handle { idx, gen_[idx] };
  }

  Handle Insert(Hot hot, Cold cold) {
    uint32_t idx = this->AllocateSlot();
    hot_[idx]  = std::move(hot);
    cold_[idx] = std::move(cold);
    ++live_count_;
    return Handle { idx, gen_[idx] };
  }

  bool Erase(Handle h) {
    if (!IsAlive(h)) return false;

    uint32_t idx = h.index;

    // Destroy Hot/Cold entries.
    hot_[idx].reset();
    cold_[idx].reset();

    // Advance generation to invalidate stale handles.
    uint32_t g = gen_[idx] + 1u;
    if (g == 0u) g = 1u;
    gen_[idx] = g;

    freelist_.emplace_back(idx);
    --live_count_;
    return true;
  }

  bool IsAlive(Handle h) const {
    if (h.IsNull()) return false;
    if (h.index >= gen_.size()) return false;
    if (gen_[h.index] != h.generation) return false;
    // Treat as dead if either Hot or Cold is missing.
    return hot_[h.index].has_value() && cold_[h.index].has_value();
  }

  Hot* HotPtr(Handle h) {
    if (!IsAlive(h)) return nullptr;
    return &*hot_[h.index];
  }
  Cold* ColdPtr(Handle h) {
    if (!IsAlive(h)) return nullptr;
    return &*cold_[h.index];
  }
  Hot const* HotPtr(Handle h) const {
    if (!IsAlive(h)) return nullptr;
    return &*hot_[h.index];
  }
  Cold const* ColdPtr(Handle h) const {
    if (!IsAlive(h)) return nullptr;
    return &*cold_[h.index];
  }

  Hot& HotRef(Handle h) {
    MBASE_ASSERT(IsAlive(h));
    return *hot_[h.index];
  }
  Cold& ColdRef(Handle h) {
    MBASE_ASSERT(IsAlive(h));
    return *cold_[h.index];
  }
  Hot const& HotRef(Handle h) const {
    MBASE_ASSERT(IsAlive(h));
    return *hot_[h.index];
  }
  Cold const& ColdRef(Handle h) const {
    MBASE_ASSERT(IsAlive(h));
    return *cold_[h.index];
  }

  // Destroy live entries.
  void Clear() {
    for (uint32_t i = 0; i < gen_.size(); ++i) {
      if (hot_[i].has_value()) {
        hot_[i].reset();
        cold_[i].reset();
        uint32_t g = gen_[i] + 1u;
        if (g == 0u) g = 1u;
        gen_[i] = g;
        freelist_.emplace_back(i);
      }
    }
    live_count_ = 0;
  }

  // O(slot_count)
  template <class F>
  void ForEachAlive(F&& f) {
    for (uint32_t i = 0; i < gen_.size(); ++i) {
      if (!hot_[i].has_value()) continue;
      // Generate `Handle` on-the-fly.
      Handle h { i, gen_[i] };
      f(h, *hot_[i], *cold_[i]);
    }
  }

private:
  uint32_t AllocateSlot() {
    if (!freelist_.empty()) {
      uint32_t idx = freelist_.back();
      freelist_.pop_back();
      return idx;
    }

    // Add a new slot
    uint32_t idx = static_cast<uint32_t>(gen_.size());
    gen_.emplace_back(1u);               // generation starts from 1
    hot_.emplace_back(std::nullopt);
    cold_.emplace_back(std::nullopt);
    return idx;
  }

  std::vector<std::optional<Hot>>  hot_;
  std::vector<std::optional<Cold>> cold_;
  std::vector<uint32_t> gen_;
  std::vector<uint32_t> freelist_;
  uint32_t live_count_ = 0;
};

} // namespace container
