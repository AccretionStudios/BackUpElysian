#pragma once
#include <glm/fwd.hpp>
#include <vulkan/vulkan.h>
#include "GBuffer.h"
#include "Core/VulkanContext.h"

namespace Elysian
{
    class GeometryPass
    {
    public:
        void Init(VulkanContext* context, GBuffer* gbuffer, VkDescriptorSetLayout meshDescriptorSetLayout);
        void Cleanup();

        void Record(VkCommandBuffer cmdBuffer, uint32_t currentFrame, VkDescriptorSet meshDescriptorSet,
                    VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t indexCount, const glm::mat4& model,
                    VkFrontFace frontFace);
        
        VkPipeline GetPipelineCCW() const { return m_PipelineCCW; }
        VkPipeline GetPipelineCW()  const { return m_PipelineCW; }
        VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }

    private:
        VulkanContext* m_Context = nullptr;
        GBuffer* m_GBuffer = nullptr;

        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_PipelineCCW = VK_NULL_HANDLE;
        VkPipeline m_PipelineCW = VK_NULL_HANDLE;

        void CreatePipeline(VkDescriptorSetLayout meshDescriptorSetLayout);
    };
}
