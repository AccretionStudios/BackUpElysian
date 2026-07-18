#pragma once
#include "Core/Camera.h"
#include "Core/Mesh.h"
#include "ECS/ECSManager.h"
#include <glm/glm.hpp>

namespace Elysian
{
    class Scene
    {
    public:
        void Init();

        Camera m_Camera{glm::vec3(0.0f, 0.0f, 5.0f)};
        Mesh m_Mesh; // primary mesh asset

        ECSManager m_ECSManager;

        // Light data removed – now in ECS
        float m_AmbientStrength = 0.1f;

        entt::entity GetMainEntity() const { return m_MainEntity; }

    private:
        entt::entity m_MainEntity;
    };
}
