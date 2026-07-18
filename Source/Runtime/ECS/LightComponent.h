#pragma once

#include "glm/glm.hpp"

namespace Elysian
{
    enum class LightType
    {
        Directional,
        Point,
        Spot
    };

    struct LightComponent
    {
        LightType type = LightType::Point;
        glm::vec3 color{1.0f};
        float intensity = 1.0f;
        float radius = 5.0f; // attenuation distance (for point/spot)
        float innerAngle = glm::radians(20.0f); // spot only
        float outerAngle = glm::radians(35.0f); // spot only
    };
}
