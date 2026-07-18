#pragma once
#include <memory>

namespace Elysian
{
    class Mesh;

    struct MeshComponent
    {
        Mesh* mesh = nullptr;
        uint32_t vertexOffset = 0;   // Starting vertex index in the global buffer
        uint32_t indexOffset = 0;    // Starting index in the global index buffer
        uint32_t indexCount = 0;     // How many indices this mesh uses

        MeshComponent() = default;
        MeshComponent(Mesh* meshPtr) : mesh(meshPtr) {}
    };
}
