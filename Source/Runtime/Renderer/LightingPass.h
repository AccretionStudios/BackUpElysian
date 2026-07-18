#pragma once

#include <vulkan/vulkan.h>
#include "GBuffer.h"
#include "Core/VulkanContext.h"

namespace Elysian
{
    class LightingPass
    {
    public:
        void Init(VulkanContext* context, GBuffer* gbuffer, VkRenderPass swapchainRenderPass);
        void Cleanup();

        // Records the fullscreen draw. Assumes both descriptor sets are already bound.
        void Record(VkCommandBuffer cmdBuffer);

        VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }
        VkPipeline GetPipeline() const { return m_Pipeline; }

    private:
        VulkanContext* m_Context = nullptr;
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_Pipeline = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_GBufferDescLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_LightDescLayout = VK_NULL_HANDLE;

        void CreatePipeline(VkRenderPass swapchainRenderPass);
    };
}
