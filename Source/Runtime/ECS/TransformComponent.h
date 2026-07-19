#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Elysian
{
    struct TransformComponent
    {
        glm::vec3 Position{0.0f};
        glm::vec3 Rotation{0.0f}; // Euler angles
        glm::vec3 Scale{1.0f};

        glm::mat4 GetModelMatrix() const
        {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), Position);
            model = glm::rotate(model, glm::radians(Rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::rotate(model, glm::radians(Rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, glm::radians(Rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, Scale);
            return model;
        }
    };
}
