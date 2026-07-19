#include "Camera.h"
#include <imgui/imgui.h>
#include <algorithm>
#include <cmath>

namespace Elysian
{
    Camera::Camera(glm::vec3 position) : Position(position), WorldUp(glm::vec3(0.0f, 1.0f, 0.0f)), m_LastX(640), m_LastY(360)
    {
        UpdateCameraVectors();
    }

    void Camera::Update(GLFWwindow* window, float deltaTime)
    {
        ImGuiIO& io = ImGui::GetIO();
        float velocity = MovementSpeed * deltaTime;

        // Detect Alt key
        bool altPressed = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;

        // Mouse buttons
        bool lmbPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        bool rmbPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

        // MOUSE INPUT
        if (!io.WantCaptureMouse)
        {
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);

            if (m_IsFirstMouse)
            {
                m_LastX = xpos;
                m_LastY = ypos;
                m_IsFirstMouse = false;
            }

            float xoffset = static_cast<float>(xpos - m_LastX) * MouseSensitivity;
            float yoffset = static_cast<float>(m_LastY - ypos) * MouseSensitivity;
            m_LastX = xpos;
            m_LastY = ypos;

            // Orbit (Alt + LMB)
            if (altPressed && lmbPressed)
            {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

                if (!m_OrbitPivotLocked)
                {
                    m_OrbitPivot = Position + Forward * 5.0f;
                    m_OrbitRadius = glm::distance(Position, m_OrbitPivot);
                    m_OrbitYaw = Yaw;
                    m_OrbitPitch = Pitch;
                    m_OrbitPivotLocked = true;
                }
                m_IsOrbiting = true;

                // Update yaw/pitch from mouse movement
                m_OrbitYaw += xoffset;
                m_OrbitPitch += yoffset;

                // Clamp pitch
                if (m_OrbitPitch > 89.0f) m_OrbitPitch = 89.0f;
                if (m_OrbitPitch < -89.0f) m_OrbitPitch = -89.0f;

                Yaw = m_OrbitYaw;
                Pitch = m_OrbitPitch;

                UpdateOrbitPosition();
                UpdateCameraVectors();
            }
            // Camera dolly (Alt + RMB)
            else if (altPressed && rmbPressed)
            {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

                if (!m_IsDollying)
                {
                    m_IsDollying = true;
                }
                float delta = -yoffset * DollySensitivity;
                if (m_OrbitPivotLocked)
                {
                    // In orbit mode, dolly changes radius – pivot stays fixed
                    m_OrbitRadius -= delta;
                    if (m_OrbitRadius < 0.1f) m_OrbitRadius = 0.1f;
                    UpdateOrbitPosition();
                    UpdateCameraVectors();
                }
                else
                {
                    Position += Forward * delta;
                }
            }
            // Regular look (RMB only, no Alt)
            else if (rmbPressed && !altPressed)
            {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

                Yaw += xoffset;
                Pitch += yoffset;
                if (Pitch > 89.0f) Pitch = 89.0f;
                if (Pitch < -89.0f) Pitch = -89.0f;
                UpdateCameraVectors();
            }
            else
            {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }

            // Reset orbit state only when both Alt and LMB are released
            if (!altPressed || !lmbPressed)
            {
                if (m_IsOrbiting)
                {
                    m_IsOrbiting = false;
                }
                // Clear the pivot lock only when both Alt AND LMB are fully released
                if (!altPressed && !lmbPressed)
                {
                    m_OrbitPivotLocked = false;
                }
            }
            if (!altPressed || !rmbPressed)
            {
                m_IsDollying = false;
            }
        }

        // Keyboard inputs
        bool allowMovement = rmbPressed && !altPressed && !m_OrbitPivotLocked;
        if (allowMovement)
        {
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) Position += Forward * velocity;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) Position -= Forward * velocity;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) Position -= Right * velocity;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) Position += Right * velocity;
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) Position -= WorldUp * velocity;
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) Position += WorldUp * velocity;
        }

        // Gamepad inputs (Untested)
        if (!m_OrbitPivotLocked && !altPressed)
        {
            GLFWgamepadstate state;
            if (glfwGetGamepadState(GLFW_JOYSTICK_1, &state))
            {
                float moveX = state.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
                float moveY = state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
                if (std::abs(moveX) > Deadzone) Position += Right * moveX * velocity;
                if (std::abs(moveY) > Deadzone) Position -= Forward * moveY * velocity;

                float lookX = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
                float lookY = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];
                if (std::abs(lookX) > Deadzone) Yaw += lookX * GamepadSensitivity * deltaTime;
                if (std::abs(lookY) > Deadzone) Pitch -= lookY * GamepadSensitivity * deltaTime;

                if (state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] == GLFW_PRESS) Position += WorldUp * velocity;
                if (state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] == GLFW_PRESS) Position -= WorldUp * velocity;
            }
        }

        // Clamp pitch and update vectors
        if (Pitch > 89.0f) Pitch = 89.0f;
        if (Pitch < -89.0f) Pitch = -89.0f;
        UpdateCameraVectors();

        // Reset first mouse flag when no mouse look mode is active
        bool mouseLookActive = (rmbPressed && !altPressed) ||
            (altPressed && lmbPressed) ||
            (altPressed && rmbPressed);
        if (!mouseLookActive)
        {
            m_IsFirstMouse = true;
        }
    }

    void Camera::ProcessScroll(float yoffset)
    {
        if (m_OrbitPivotLocked)
        {
            // Scroll while orbiting changes radius, and pivot stays absolutely fixed
            m_OrbitRadius -= yoffset * ScrollSensitivity;
            if (m_OrbitRadius < 0.1f) m_OrbitRadius = 0.1f;
            UpdateOrbitPosition();
            UpdateCameraVectors();
        }
        else
        {
            Position += Forward * yoffset * ScrollSensitivity;
        }
    }

    void Camera::UpdateCameraVectors()
    {
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));

        Forward = glm::normalize(front);
        Right = glm::normalize(glm::cross(Forward, WorldUp));
        Up = glm::normalize(glm::cross(Right, Forward));
    }

    void Camera::UpdateOrbitPosition()
    {
        glm::vec3 dir;
        dir.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        dir.y = sin(glm::radians(Pitch));
        dir.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        dir = glm::normalize(dir);
        // Camera position = fixed pivot - radius * direction
        Position = m_OrbitPivot - dir * m_OrbitRadius;
    }

    glm::mat4 Camera::GetViewMatrix() const
    {
        return glm::lookAt(Position, Position + Forward, Up);
    }

    glm::mat4 Camera::GetProjectionMatrix(float aspect) const
    {
        auto proj = glm::perspective(glm::radians(FieldOfView), aspect, 0.1f, 100.0f);
        proj[1][1] *= -1;
        return proj;
    }
}
