#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

namespace Elysian
{
    class Camera
    {
    public:
        glm::vec3 Forward;
        glm::vec3 Up;
        glm::vec3 Right;
        glm::vec3 WorldUp;

        Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, -5.0f));

        void Update(GLFWwindow* window, float deltaTime);
        void ProcessScroll(float yoffset);

        glm::mat4 GetViewMatrix() const;
        glm::mat4 GetProjectionMatrix(float aspect) const;

        glm::vec3 Position;
        float Yaw = -90.0f;
        float Pitch = 0.0f;
        float MovementSpeed = 2.5f;
        float MouseSensitivity = 0.1f;
        float GamepadSensitivity = 100.0f;
        float Deadzone = 0.15f;
        float ScrollSensitivity = 0.5f;
        float DollySensitivity = 0.05f;
        float FieldOfView = 45.0f;

    private:
        bool m_IsFirstMouse = true;
        double m_LastX, m_LastY;
        bool m_IsOrbiting = false;
        bool m_IsDollying = false;
        glm::vec3 m_OrbitPivot;
        bool m_OrbitPivotLocked = false;
        float m_OrbitRadius = 100.0f;
        float m_OrbitYaw, m_OrbitPitch;

        void UpdateCameraVectors();
        void UpdateOrbitPosition();
    };
}
