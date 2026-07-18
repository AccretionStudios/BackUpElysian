#pragma once
#include "Mesh.h"
#include <unordered_map>
#include <string>
#include <memory>

namespace Elysian {

    class ResourceManager {
    public:
        static ResourceManager& Get() {
            static ResourceManager instance;
            return instance;
        }

        Mesh* LoadMesh(const std::string& path) {
            auto it = m_Meshes.find(path);
            if (it != m_Meshes.end()) {
                return it->second.get();
            }

            auto mesh = std::make_unique<Mesh>();
            if (!mesh->LoadFromFile(path)) {
                return nullptr; // or throw
            }
            Mesh* ptr = mesh.get();
            m_Meshes[path] = std::move(mesh);
            return ptr;
        }
        
        void Clear() {
            m_Meshes.clear();
        }

    private:
        std::unordered_map<std::string, std::unique_ptr<Mesh>> m_Meshes;
        ResourceManager() = default;
        ~ResourceManager() = default;
        ResourceManager(const ResourceManager&) = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;
    };

} // namespace Elysian