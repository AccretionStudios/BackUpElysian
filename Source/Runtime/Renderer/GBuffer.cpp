#include "GBuffer.h"
#include <stdexcept>

namespace Elysian
{
    void GBuffer::Init(VulkanContext* context, const GBufferCreateInfo& createInfo)
    {
        m_Context = context;
        m_Extent = {createInfo.width, createInfo.height};

        CreateImages();
        CreateImageViews();
        CreateRenderPass();
    }

    void GBuffer::Cleanup()
    {
        VkDevice device = m_Context->GetDevice();
        VmaAllocator allocator = m_Context->GetAllocator();

        if (m_RenderPass) vkDestroyRenderPass(device, m_RenderPass, nullptr);

        for (int i = 0; i < 3; ++i)
        {
            if (m_ImageViews[i]) vkDestroyImageView(device, m_ImageViews[i], nullptr);
            if (m_Images[i]) vmaDestroyImage(allocator, m_Images[i], m_Allocations[i]);
        }
        if (m_DepthImageView) vkDestroyImageView(device, m_DepthImageView, nullptr);
        if (m_DepthImage) vmaDestroyImage(allocator, m_DepthImage, m_DepthAllocation);
    }

    void GBuffer::CreateImages()
    {
        VkFormat colorFormats[3] = {
            VK_FORMAT_R8G8B8A8_UNORM, // Albedo
            VK_FORMAT_R16G16B16A16_SFLOAT, // Normal
            VK_FORMAT_R16G16B16A16_SFLOAT // World Position
        };
        VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        for (int i = 0; i < 3; ++i)
        {
            m_Context->CreateImageVMA(m_Extent.width, m_Extent.height, colorFormats[i],
                                      usage, VMA_MEMORY_USAGE_GPU_ONLY, m_Images[i], m_Allocations[i]);
        }

        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
        m_Context->CreateImageVMA(m_Extent.width, m_Extent.height, depthFormat,
                                  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VMA_MEMORY_USAGE_GPU_ONLY,
                                  m_DepthImage, m_DepthAllocation);
    }

    void GBuffer::CreateImageViews()
    {
        VkDevice device = m_Context->GetDevice();
        VkFormat colorFormats[3] = {
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_FORMAT_R16G16B16A16_SFLOAT
        };
        for (int i = 0; i < 3; ++i)
        {
            VkImageViewCreateInfo viewInfo = {};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = m_Images[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = colorFormats[i];
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.layerCount = 1;
            if (vkCreateImageView(device, &viewInfo, nullptr, &m_ImageViews[i]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create G-buffer image view");
        }

        VkImageViewCreateInfo depthViewInfo = {};
        depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthViewInfo.image = m_DepthImage;
        depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthViewInfo.format = VK_FORMAT_D32_SFLOAT;
        depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthViewInfo.subresourceRange.levelCount = 1;
        depthViewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device, &depthViewInfo, nullptr, &m_DepthImageView) != VK_SUCCESS)
            throw std::runtime_error("Failed to create depth image view");
    }

    void GBuffer::CreateRenderPass()
    {
        VkDevice device = m_Context->GetDevice();

        std::array<VkAttachmentDescription, 4> attachments = {};
        // Albedo
        attachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        // Normal
        attachments[1].format = VK_FORMAT_R16G16B16A16_SFLOAT;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        // World Position
        attachments[2].format = VK_FORMAT_R16G16B16A16_SFLOAT;
        attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        // Depth
        attachments[3].format = VK_FORMAT_D32_SFLOAT;
        attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[3].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        std::array<VkAttachmentReference, 3> colorRefs = {
            VkAttachmentReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
            VkAttachmentReference{1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
            VkAttachmentReference{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
        };
        VkAttachmentReference depthRef = {3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
        subpass.pColorAttachments = colorRefs.data();
        subpass.pDepthStencilAttachment = &depthRef;

        // FIX: Two subpass dependencies are required, not one.
        //
        // dependencies[0] — ENTRY (EXTERNAL → subpass 0):
        //   Ensures that any previous fragment shader reads of these images (from prior
        //   frames) have completed before we start writing to the color attachments again.
        //
        // dependencies[1] — EXIT (subpass 0 → EXTERNAL):
        //   This was missing entirely. Without it there is no guarantee that the geometry
        //   pass's color attachment writes are visible to the lighting pass's fragment
        //   shader reads. The result is a GPU-level read-after-write hazard: the lighting
        //   pass may sample stale or undefined GBuffer data, causing rendering corruption
        //   or a GPU TDR crash.
        std::array<VkSubpassDependency, 2> dependencies = {};

        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo rpInfo = {};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        rpInfo.pAttachments = attachments.data();
        rpInfo.subpassCount = 1;
        rpInfo.pSubpasses = &subpass;
        rpInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        rpInfo.pDependencies = dependencies.data();

        if (vkCreateRenderPass(device, &rpInfo, nullptr, &m_RenderPass) != VK_SUCCESS)
            throw std::runtime_error("Failed to create G-buffer render pass");
    }

    VkDescriptorImageInfo GBuffer::GetAlbedoDescriptor(VkSampler sampler) const
    {
        VkDescriptorImageInfo info = {};
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        info.imageView = m_ImageViews[0];
        info.sampler = sampler;
        return info;
    }

    VkDescriptorImageInfo GBuffer::GetNormalDescriptor(VkSampler sampler) const
    {
        VkDescriptorImageInfo info = {};
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        info.imageView = m_ImageViews[1];
        info.sampler = sampler;
        return info;
    }

    VkDescriptorImageInfo GBuffer::GetWorldPosDescriptor(VkSampler sampler) const
    {
        VkDescriptorImageInfo info = {};
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        info.imageView = m_ImageViews[2];
        info.sampler = sampler;
        return info;
    }
}
