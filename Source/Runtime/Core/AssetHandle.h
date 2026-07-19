#pragma once
#include <cstdint>

namespace Elysian {

struct AssetHandle {
    // Max number unit32_t can hold
    static constexpr uint32_t INVALID_INDEX = UINT32_MAX;

    uint32_t index; // Which slot in the asset array
    uint32_t generation; // Avoid using a stale index (like a version number)

    // Default constructor that makes an invalid handle.
    AssetHandle() : index(INVALID_INDEX), generation(0) {}
    AssetHandle(uint32_t idx, uint32_t gen) : index(idx), generation(gen) {}

    // Returns true if this handle points to a valid asset.
    // A valid handle must have an index that is NOT INVALID_INDEX,
    // and a generation that is not zero (zero means "never used").
    bool IsValid() const { return index != INVALID_INDEX && generation != 0; }
    explicit operator bool() const { return IsValid(); }
    
    // Test if two handles are equal – they are equal if both index and generation match.
    bool operator==(const AssetHandle& other) const {
        return index == other.index && generation == other.generation;
    }
};

struct AssetHandleHash {
    // Pack index into upper 32 bits, and generation into lower 32/
    size_t operator()(const AssetHandle& handle) const {
        return (static_cast<uint64_t>(handle.index) << 32) | handle.generation;
    }
};

}