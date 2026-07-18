#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "Core/VulkanContext.h"
#include "Core/Window.h"
#include "VMA/vk_mem_alloc.h"

namespace Elysian
{
    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    class Swapchain
    {
    public:
        void Init(VulkanContext* context, Window* window);
        void Cleanup();
        void Recreate();

        VkSwapchainKHR GetSwapchain() const { return m_SwapChain; }
        VkFormat GetImageFormat() const { return m_SwapChainImageFormat; }
        VkExtent2D GetExtent() const { return m_SwapChainExtent; }
        VkRenderPass GetRenderPass() const { return m_RenderPass; }
        VkFramebuffer GetFramebuffer(size_t index) const { return m_SwapChainFramebuffers[index]; }
        size_t GetImageCount() const { return m_SwapChainImages.size(); }

    private:
        VulkanContext* m_Context;
        Window* m_Window;

        VkSwapchainKHR m_SwapChain;
        std::vector<VkImage> m_SwapChainImages;
        VkFormat m_SwapChainImageFormat;
        VkExtent2D m_SwapChainExtent;
        std::vector<VkImageView> m_SwapChainImageViews;

        VkImage m_DepthImage;
        VmaAllocation m_DepthImageAllocation;
        VkImageView m_DepthImageView;

        VkRenderPass m_RenderPass;
        std::vector<VkFramebuffer> m_SwapChainFramebuffers;

        void CreateSwapChain();
        void CreateImageViews();
        void CreateDepthResources();
        void CreateRenderPass();
        void CreateFramebuffers();
        void CleanupResources();

        SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
        VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
        VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
        VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
        VkFormat FindDepthFormat();
        VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
                                     VkFormatFeatureFlags features);
    };
}
