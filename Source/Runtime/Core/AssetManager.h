#pragma once
#include "AssetHandle.h"
#include "Mesh.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <functional>

namespace Elysian {

    // Base metadata for any asset type
    struct AssetEntryBase {
        uint64_t guid = 0;               // 64-bit hash of the file path
        uint32_t refCount = 0;           // How many handles point to this
        uint32_t generation = 1;         // Incremented on reload/delete
        bool isLoading = false;          // For async loading
    };

    template<typename T>
    struct AssetEntry : public AssetEntryBase {
        T data; // The actual mesh, texture, etc.
    };

    // The main manager. Singleton is acceptable here (like a global game file system).
    class AssetManager {
    public:
        static AssetManager& Get() {
            static AssetManager instance;
            return instance;
        }

        // --- Mesh API ---
        AssetHandle LoadMesh(const std::string& filepath);
        Mesh* GetMesh(const AssetHandle& handle);
        void ReleaseMesh(const AssetHandle& handle); // Decrements refcount

        // --- Cleanup ---
        void Clear();

    private:
        // Private constructor/destructor for singleton
        AssetManager() = default;
        ~AssetManager() = default;
        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;

        // Internal helpers
        uint64_t GenerateGUID(const std::string& path) const;
        bool IsHandleValid(const AssetHandle& handle, size_t registrySize) const;

        // --- Mesh Registry ---
        // Vector stores the actual data. Contiguous memory = fast iteration (BuildMegaBuffers).
        std::vector<AssetEntry<Mesh>> m_MeshRegistry;
        // Map GUID -> vector index for fast lookup.
        std::unordered_map<uint64_t, uint32_t> m_GuidToIndex;
    };

} // namespace Elysian