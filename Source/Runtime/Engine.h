#pragma once
#include "Core/Window.h"
#include "Core/VulkanContext.h"
#include "Renderer/Swapchain.h"
#include "Scene/Scene.h"
#include "Renderer/Renderer.h"
#include "Core/DebugUI.h"
#include <string>

namespace Elysian
{
    class Engine
    {
    public:
        void Init(int width, int height, const std::string& title);
        void Run();
        void Shutdown();

    private:
        Window m_Window;
        VulkanContext m_Context;
        Swapchain m_Swapchain;

        Scene m_Scene;
        Renderer m_Renderer;
        DebugUI m_DebugUI;

        float m_DeltaTime = 0.0f;
        float m_LastFrame = 0.0f;
        bool m_IsFocused = true;
        bool m_FramebufferResized = false;

        void RecreateSwapchainAndRenderer();

        static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);
        static void WindowFocusCallback(GLFWwindow* window, int focused);
        static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    };
}
