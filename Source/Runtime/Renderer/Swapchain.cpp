#include "Swapchain.h"
#include <algorithm>
#include <stdexcept>

namespace Elysian
{
    void Swapchain::Init(VulkanContext* context, Window* window)
    {
        m_Context = context;
        m_Window = window;

        CreateSwapChain();
        CreateImageViews();
        CreateDepthResources();
        CreateRenderPass();
        CreateFramebuffers();
    }

    void Swapchain::CleanupResources()
    {
        VkDevice device = m_Context->GetDevice();
        VmaAllocator allocator = m_Context->GetAllocator();

        vkDestroyImageView(device, m_DepthImageView, nullptr);
        vmaDestroyImage(allocator, m_DepthImage, m_DepthImageAllocation); // changed
        for (auto fb : m_SwapChainFramebuffers) vkDestroyFramebuffer(device, fb, nullptr);
        for (auto iv : m_SwapChainImageViews) vkDestroyImageView(device, iv, nullptr);
        vkDestroySwapchainKHR(device, m_SwapChain, nullptr);
    }

    void Swapchain::Cleanup()
    {
        CleanupResources();
        vkDestroyRenderPass(m_Context->GetDevice(), m_RenderPass, nullptr);
    }

    void Swapchain::Recreate()
    {
        int w = 0, h = 0;
        m_Window->GetFramebufferSize(w, h);
        while (w == 0 || h == 0)
        {
            m_Window->GetFramebufferSize(w, h);
            m_Window->WaitEvents();
        }
        vkDeviceWaitIdle(m_Context->GetDevice());

        CleanupResources();
        CreateSwapChain();
        CreateImageViews();
        CreateDepthResources();
        CreateFramebuffers();
    }

    void Swapchain::CreateSwapChain()
    {
        auto support = QuerySwapChainSupport(m_Context->GetPhysicalDevice());
        auto format = ChooseSwapSurfaceFormat(support.formats);
        auto mode = ChooseSwapPresentMode(support.presentModes);
        auto extent = ChooseSwapExtent(support.capabilities);

        uint32_t imgCount = support.capabilities.minImageCount + 1;
        if (support.capabilities.maxImageCount > 0 && imgCount > support.capabilities.maxImageCount)
        {
            imgCount = support.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        info.surface = m_Context->GetSurface();
        info.minImageCount = imgCount;
        info.imageFormat = format.format;
        info.imageColorSpace = format.colorSpace;
        info.imageExtent = extent;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.preTransform = support.capabilities.currentTransform;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = mode;
        info.clipped = VK_TRUE;

        if (vkCreateSwapchainKHR(m_Context->GetDevice(), &info, nullptr, &m_SwapChain) != VK_SUCCESS)
            throw std::runtime_error("failed to create swap chain!");

        vkGetSwapchainImagesKHR(m_Context->GetDevice(), m_SwapChain, &imgCount, nullptr);
        m_SwapChainImages.resize(imgCount);
        vkGetSwapchainImagesKHR(m_Context->GetDevice(), m_SwapChain, &imgCount, m_SwapChainImages.data());
        m_SwapChainImageFormat = format.format;
        m_SwapChainExtent = extent;
    }

    void Swapchain::CreateImageViews()
    {
        m_SwapChainImageViews.resize(m_SwapChainImages.size());
        for (size_t i = 0; i < m_SwapChainImages.size(); i++)
        {
            VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            info.image = m_SwapChainImages[i];
            info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            info.format = m_SwapChainImageFormat;
            info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            vkCreateImageView(m_Context->GetDevice(), &info, nullptr, &m_SwapChainImageViews[i]);
        }
    }

    void Swapchain::CreateDepthResources()
    {
        VkFormat depthFormat = FindDepthFormat();
        // Use VMA version
        m_Context->CreateImageVMA(m_SwapChainExtent.width, m_SwapChainExtent.height, depthFormat,
                                  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                  VMA_MEMORY_USAGE_GPU_ONLY,
                                  m_DepthImage, m_DepthImageAllocation); // changed variable name

        VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        info.image = m_DepthImage;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = depthFormat;
        info.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        vkCreateImageView(m_Context->GetDevice(), &info, nullptr, &m_DepthImageView);
    }

    void Swapchain::CreateRenderPass()
    {
        VkAttachmentDescription colorAtt{
            0, m_SwapChainImageFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        };
        VkAttachmentDescription depthAtt{
            0, FindDepthFormat(), VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };
        VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;
        VkSubpassDependency dependency{
            VK_SUBPASS_EXTERNAL, 0,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
        };
        std::vector<VkAttachmentDescription> atts = {colorAtt, depthAtt};
        VkRenderPassCreateInfo info{
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr, 0, (uint32_t)atts.size(), atts.data(), 1, &subpass, 1,
            &dependency
        };
        vkCreateRenderPass(m_Context->GetDevice(), &info, nullptr, &m_RenderPass);
    }

    void Swapchain::CreateFramebuffers()
    {
        m_SwapChainFramebuffers.resize(m_SwapChainImageViews.size());
        for (size_t i = 0; i < m_SwapChainImageViews.size(); i++)
        {
            std::vector<VkImageView> atts = {m_SwapChainImageViews[i], m_DepthImageView};
            VkFramebufferCreateInfo info{
                VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, nullptr, 0, m_RenderPass, (uint32_t)atts.size(), atts.data(),
                m_SwapChainExtent.width, m_SwapChainExtent.height, 1
            };
            vkCreateFramebuffer(m_Context->GetDevice(), &info, nullptr, &m_SwapChainFramebuffers[i]);
        }
    }

    SwapChainSupportDetails Swapchain::QuerySwapChainSupport(VkPhysicalDevice device)
    {
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Context->GetSurface(), &details.capabilities);
        uint32_t fCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Context->GetSurface(), &fCount, nullptr);
        if (fCount != 0)
        {
            details.formats.resize(fCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Context->GetSurface(), &fCount, details.formats.data());
        }
        uint32_t pCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Context->GetSurface(), &pCount, nullptr);
        if (pCount != 0)
        {
            details.presentModes.resize(pCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Context->GetSurface(), &pCount,
                                                      details.presentModes.data());
        }
        return details;
    }

    VkSurfaceFormatKHR Swapchain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
    {
        for (const auto& f : availableFormats) if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace ==
            VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return f;
        return availableFormats[0];
    }

    VkPresentModeKHR Swapchain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes)
    {
        for (const auto& m : modes) if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D Swapchain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& caps)
    {
        if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) return caps.currentExtent;
        int w, h;
        m_Window->GetFramebufferSize(w, h);
        VkExtent2D actual = {(uint32_t)w, (uint32_t)h};
        actual.width = std::clamp(actual.width, caps.minImageExtent.width, caps.maxImageExtent.width);
        actual.height = std::clamp(actual.height, caps.minImageExtent.height, caps.maxImageExtent.height);
        return actual;
    }

    VkFormat Swapchain::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
                                            VkFormatFeatureFlags features)
    {
        for (VkFormat format : candidates)
        {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(m_Context->GetPhysicalDevice(), format, &props);
            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) return format;
            else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) return
                format;
        }
        throw std::runtime_error("failed to find supported format!");
    }

    VkFormat Swapchain::FindDepthFormat()
    {
        return FindSupportedFormat(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }
}
