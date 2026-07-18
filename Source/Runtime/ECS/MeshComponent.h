#pragma once
#include <memory>

namespace Elysian
{
    class Mesh;

    struct MeshComponent
    {
        Mesh* mesh = nullptr;
    };
}
