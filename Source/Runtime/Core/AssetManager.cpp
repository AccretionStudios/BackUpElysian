// Core/AssetManager.cpp
#include "AssetManager.h"
#include <functional> // for std::hash
#include <iostream>

namespace Elysian {
    
// GUID Generation (Hash of absolute/relative path)
uint64_t AssetManager::GenerateGUID(const std::string& path) const {
    // Simple but effective. In AAA you'd use SHA-1 or a dedicated asset GUID.
    std::hash<std::string> hasher;
    return static_cast<uint64_t>(hasher(path));
}

// Load Mesh
AssetHandle AssetManager::LoadMesh(const std::string& filepath) {
    uint64_t guid = GenerateGUID(filepath);

    // 1. Check if already loaded
    auto it = m_GuidToIndex.find(guid);
    if (it != m_GuidToIndex.end()) {
        uint32_t idx = it->second;
        // Safety check: ensure the entry exists and generation hasn't wrapped (unlikely)
        if (idx < m_MeshRegistry.size()) {
            m_MeshRegistry[idx].refCount++;
            return AssetHandle(idx, m_MeshRegistry[idx].generation);
        } else {
            // Corruption recovery: erase from map and reload
            m_GuidToIndex.erase(it);
        }
    }

    // 2. Load new mesh
    AssetEntry<Mesh> newEntry;
    newEntry.guid = guid;
    newEntry.generation = 1; // Start at 1 (0 is considered invalid)
    newEntry.refCount = 1;
    newEntry.isLoading = false;

    // 3. Actually load the file (blocking for now, async later)
    if (!newEntry.data.LoadFromFile(filepath)) {
        return AssetHandle(); // Invalid handle
    }

    // 4. Store in registry
    uint32_t newIndex = static_cast<uint32_t>(m_MeshRegistry.size());
    m_MeshRegistry.push_back(std::move(newEntry));
    m_GuidToIndex[guid] = newIndex;

    return AssetHandle(newIndex, 1);
}
    
// Get Mesh (Resolve Handle)
Mesh* AssetManager::GetMesh(const AssetHandle& handle) {
    if (!IsHandleValid(handle, m_MeshRegistry.size())) {
        return nullptr;
    }

    const auto& entry = m_MeshRegistry[handle.index];
    
    // Check if the handle's generation matches the current generation
    // If not, the asset was reloaded or removed => handle is stale.
    if (entry.generation != handle.generation) {
        std::cerr << "[AssetManager] STALE HANDLE DETECTED! (Index: " 
                  << handle.index << ", Expected Gen: " << entry.generation 
                  << ", Got Gen: " << handle.generation << ")" << std::endl;
        return nullptr;
    }

    if (entry.refCount == 0) {
        // Shouldn't happen if generation matches, but safety check.
        return nullptr;
    }

    return const_cast<Mesh*>(&entry.data);
}
    
// Release Mesh (Decrement RefCount)
void AssetManager::ReleaseMesh(const AssetHandle& handle) {
    if (!IsHandleValid(handle, m_MeshRegistry.size())) return;

    auto& entry = m_MeshRegistry[handle.index];
    if (entry.generation != handle.generation) return; // stale, ignore

    if (entry.refCount > 0) {
        entry.refCount--;
        // In a true AAA system, if refCount hits 0, we'd schedule it for unloading
        // or move it to a "warm cache". For now, we just leave it loaded 
        // (but we know no one is using it).
        std::cout << "[AssetManager] Mesh refCount: " << entry.refCount << std::endl;
    }
}
    
// Clear All Assets
void AssetManager::Clear() {
    m_MeshRegistry.clear();
    m_GuidToIndex.clear();
}


// Internal Validation
bool AssetManager::IsHandleValid(const AssetHandle& handle, size_t registrySize) const {
    return handle.IsValid() && handle.index < registrySize;
}

} // namespace Elysian