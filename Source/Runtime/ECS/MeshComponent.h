#pragma once
#include <memory>
#include "Core/AssetHandle.h"

namespace Elysian
{
    class Mesh;

    struct MeshComponent
    {
        AssetHandle meshHandle;;
        uint32_t vertexOffset = 0;   // Starting vertex index in the global buffer
        uint32_t indexOffset = 0;    // Starting index in the global index buffer
        uint32_t indexCount = 0;     // How many indices this mesh uses
    };
}
