#pragma once
#include "Core/VulkanContext.h"
#include "Renderer/Swapchain.h"
#include "Renderer/GBuffer.h"
#include "Renderer/GeometryPass.h"
#include "Renderer/LightingPass.h"
#include "Renderer/MeshRenderer.h"
#include "Scene/Scene.h"
#include "Core/DebugUI.h"
#include <vector>

namespace Elysian
{
    class Renderer
    {
    public:
        void Init(VulkanContext* context, Swapchain* swapchain, Scene* scene);
        void Cleanup();

        bool DrawFrame(Scene* scene, DebugUI* ui, Swapchain* swapchain);
        void RecreateResolutionDependentResources(Swapchain* swapchain);
        
        void CreateGridPipeline();
        VkPipeline m_GridPipeline = VK_NULL_HANDLE;
        VkPipelineLayout m_GridPipelineLayout = VK_NULL_HANDLE;

    private:
        VulkanContext* m_Context = nullptr;
        Swapchain* m_Swapchain = nullptr;

        MeshRenderer m_MeshRenderer;

        GBuffer m_GBuffer;
        GeometryPass m_GeometryPass;
        LightingPass m_LightingPass;

        VkImage m_HdrImage = VK_NULL_HANDLE;
        VmaAllocation m_HdrAllocation = VK_NULL_HANDLE;
        VkImageView m_HdrImageView = VK_NULL_HANDLE;
        VkRenderPass m_HdrRenderPass = VK_NULL_HANDLE;
        VkFramebuffer m_HdrFramebuffer = VK_NULL_HANDLE;

        VkPipelineLayout m_PresentPipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_PresentPipeline = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_PresentDescLayout = VK_NULL_HANDLE;
        VkDescriptorPool m_PresentDescPool = VK_NULL_HANDLE;
        VkDescriptorSet m_PresentDescSet = VK_NULL_HANDLE;
        VkSampler m_HdrSampler = VK_NULL_HANDLE;

        VkDescriptorSetLayout m_GBufferDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool m_LightingDescriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet m_LightingDescriptorSet;
        VkSampler m_GBufferSampler = VK_NULL_HANDLE;

        std::vector<VkCommandBuffer> m_CommandBuffers;
        std::vector<VkSemaphore> m_ImageAvailableSemaphores;
        std::vector<VkSemaphore> m_RenderFinishedSemaphores;
        std::vector<VkFence> m_InFlightFences;

        const int MAX_FRAMES_IN_FLIGHT = 2;
        uint32_t m_CurrentFrame = 0;

        void CreateGBufferDescriptorSet();
        void CreateHdrResources();
        void DestroyHdrResources();
        void CreatePresentPipeline();
        void DestroyPresentPipeline();
        void CreateCommandBuffers();
        void CreateSyncObjects();
    };
}
