#pragma once
#include <vulkan/vulkan.h>
#include "Core/VulkanContext.h"
#include "Core/Window.h"
#include "Renderer/Swapchain.h"
#include "Scene/Scene.h"
#include <entt/entt.hpp>

namespace Elysian
{
    class DebugUI
    {
    public:
        void Init(VulkanContext* context, Window* window, Swapchain* swapchain);
        void Cleanup(VulkanContext* context);

        void BeginFrame();
        void DrawWindows(Scene* scene, Swapchain* swapchain, float deltaTime);
        void Render(VkCommandBuffer cmdBuffer);

        // Called from Engine to set viewport size for ImGuizmo
        void SetViewportSize(int width, int height)
        {
            m_ViewportWidth = width;
            m_ViewportHeight = height;
        }

    private:
        VkDescriptorPool m_ImGuiDescriptorPool = VK_NULL_HANDLE;
        float m_FrameTimeHistory[100] = {0};
        int m_FrameTimeOffset = 0;
        float m_MaxFrameTime = 0.0f;
        float m_AvgFps = 60.0f;

        // Editor state
        entt::entity m_SelectedEntity = entt::null;
        int m_GizmoOperation = 0; // 0=translate, 1=rotate, 2=scale
        int m_GizmoMode = 0; // 0=local, 1=world

        int m_ViewportWidth = 1280;
        int m_ViewportHeight = 720;
    };
}
