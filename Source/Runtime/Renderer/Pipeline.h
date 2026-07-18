#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include "Core/VulkanContext.h"
#include "Swapchain.h"
#include "VulkanTypes.h"

namespace Elysian
{
    class Pipeline
    {
    public:
        void Init(VulkanContext* context, Swapchain* swapchain, VkDescriptorSetLayout descriptorSetLayout);
        void Cleanup();

        VkPipeline GetGraphicsPipeline() const { return m_GraphicsPipeline; }
        VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }

        // Static helpers for shader loading
        static std::vector<char> ReadFile(const std::string& filename);
        static VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char>& code);

    private:
        VulkanContext* m_Context;

        VkPipelineLayout m_PipelineLayout;
        VkPipeline m_GraphicsPipeline;
    };
}
