#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "Renderer/VulkanTypes.h"

namespace Elysian
{
#pragma pack(push, 1)
    struct VMSHHeader
    {
        char magic[4];
        uint32_t version; // Currently 1
        uint32_t vertexCount;
        uint32_t indexCount;
    };
#pragma pack(pop)

    class Mesh
    {
    public:
        bool LoadFromFile(const std::string& filename);

        const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
        const std::vector<uint32_t>& GetIndices() const { return m_Indices; }

        bool IsLoaded() const { return !m_Vertices.empty(); }

    private:
        std::vector<Vertex> m_Vertices;
        std::vector<uint32_t> m_Indices;

        // Internal Asset Pipeline
        bool LoadEMSH(const std::string& filename);
        bool SaveEMSH(const std::string& filename);

        bool ImportOBJ(const std::string& filename);
        bool ImportFBX(const std::string& filename);
        bool ImportGLTF(const std::string& filename);
    };
}
