#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include "Core/VulkanContext.h"

namespace Elysian
{
    struct GBufferCreateInfo
    {
        uint32_t width;
        uint32_t height;
    };

    class GBuffer
    {
    public:
        void Init(VulkanContext* context, const GBufferCreateInfo& createInfo);
        void Cleanup();

        VkRenderPass GetRenderPass() const { return m_RenderPass; }
        const std::array<VkImageView, 3>& GetImageViews() const { return m_ImageViews; }
        VkImageView GetDepthImageView() const { return m_DepthImageView; }
        VkExtent2D GetExtent() const { return m_Extent; }

        VkDescriptorImageInfo GetAlbedoDescriptor(VkSampler sampler) const;
        VkDescriptorImageInfo GetNormalDescriptor(VkSampler sampler) const;
        VkDescriptorImageInfo GetWorldPosDescriptor(VkSampler sampler) const;

    private:
        VulkanContext* m_Context = nullptr;
        VkExtent2D m_Extent = {};

        std::array<VkImage, 3> m_Images = {};
        std::array<VmaAllocation, 3> m_Allocations = {};
        std::array<VkImageView, 3> m_ImageViews = {};

        VkImage m_DepthImage = VK_NULL_HANDLE;
        VmaAllocation m_DepthAllocation = VK_NULL_HANDLE;
        VkImageView m_DepthImageView = VK_NULL_HANDLE;

        VkRenderPass m_RenderPass = VK_NULL_HANDLE;

        void CreateRenderPass();
        void CreateImages();
        void CreateImageViews();
    };
}
