#include "Engine.h"
#include <iostream>
#include <imgui/imgui.h>
#include <thread>

namespace Elysian
{
    void Engine::Init(int width, int height, const std::string& title)
    {
        try
        {
            m_Window.Init(width, height, title);
            glfwSetWindowUserPointer(m_Window.GetNativeWindow(), this);
            glfwSetFramebufferSizeCallback(m_Window.GetNativeWindow(), FramebufferResizeCallback);
            glfwSetScrollCallback(m_Window.GetNativeWindow(), ScrollCallback);
            glfwSetWindowFocusCallback(m_Window.GetNativeWindow(), WindowFocusCallback);

            m_Context.Init(&m_Window);
            m_Swapchain.Init(&m_Context, &m_Window);

            m_Scene.Init();
            m_Renderer.Init(&m_Context, &m_Swapchain, &m_Scene);
            m_DebugUI.Init(&m_Context, &m_Window, &m_Swapchain);

            std::cout << "Engine Initialization completed successfully." << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cerr << "FATAL ERROR during engine initialization: " << e.what() << std::endl;
            throw;
        }
    }

    void Engine::Run()
    {
        while (!m_Window.ShouldClose())
        {
            float currentFrame = static_cast<float>(glfwGetTime());
            m_DeltaTime = currentFrame - m_LastFrame;
            m_LastFrame = currentFrame;

            m_Window.PollEvents();
            if (!m_IsFocused)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            m_Scene.m_Camera.Update(m_Window.GetNativeWindow(), m_DeltaTime);

            int w, h;
            m_Window.GetFramebufferSize(w, h);
            m_DebugUI.SetViewportSize(w, h);

            if (m_FramebufferResized || !m_Renderer.DrawFrame(&m_Scene, &m_DebugUI, &m_Swapchain))
            {
                RecreateSwapchainAndRenderer();
                m_FramebufferResized = false;
            }
        }
        vkDeviceWaitIdle(m_Context.GetDevice());
    }

    void Engine::RecreateSwapchainAndRenderer()
    {
        vkDeviceWaitIdle(m_Context.GetDevice());
        // Clean up UI and Renderer swapchain dependencies
        m_Swapchain.Recreate();
        m_Renderer.RecreateResolutionDependentResources(&m_Swapchain);
    }

    void Engine::Shutdown()
    {
        vkDeviceWaitIdle(m_Context.GetDevice());

        m_DebugUI.Cleanup(&m_Context);
        m_Renderer.Cleanup();
        m_Swapchain.Cleanup();
        m_Context.Cleanup();
        m_Window.Cleanup();
    }

    void Engine::FramebufferResizeCallback(GLFWwindow* window, int width, int height)
    {
        auto app = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
        app->m_FramebufferResized = true;
    }

    void Engine::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
    {
        auto app = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse) return;
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) return;
        app->m_Scene.m_Camera.ProcessScroll(static_cast<float>(yoffset));
    }

    void Engine::WindowFocusCallback(GLFWwindow* window, int focused)
    {
        auto app = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
        app->m_IsFocused = focused;
    }
}
