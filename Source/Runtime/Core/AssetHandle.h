#pragma once
#include <cstdint>

namespace Elysian {

struct AssetHandle {
    static constexpr uint32_t INVALID_INDEX = UINT32_MAX;

    uint32_t index;
    uint32_t generation;

    AssetHandle() : index(INVALID_INDEX), generation(0) {}
    AssetHandle(uint32_t idx, uint32_t gen) : index(idx), generation(gen) {}

    bool IsValid() const { return index != INVALID_INDEX && generation != 0; }
    explicit operator bool() const { return IsValid(); }

    bool operator==(const AssetHandle& other) const {
        return index == other.index && generation == other.generation;
    }
};

struct AssetHandleHash {
    size_t operator()(const AssetHandle& handle) const {
        return (static_cast<uint64_t>(handle.index) << 32) | handle.generation;
    }
};

} // namespace Elysian