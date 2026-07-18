#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <optional>
#include <string>
#include "VMA/vk_mem_alloc.h"

namespace Elysian
{
    class Window;

    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
    };

    class VulkanContext
    {
    public:
        void Init(Window* window);
        void Cleanup();

        VkInstance GetInstance() const { return m_Instance; }
        VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
        VkDevice GetDevice() const { return m_Device; }
        VkSurfaceKHR GetSurface() const { return m_Surface; }
        VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
        VkQueue GetPresentQueue() const { return m_PresentQueue; }
        VkCommandPool GetCommandPool() const { return m_CommandPool; }
        VmaAllocator GetAllocator() const { return m_Allocator; }

        QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
        uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

        void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                         VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image,
                         VkDeviceMemory& imageMemory);
        void CreateImageVMA(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage,
                            VmaMemoryUsage memoryUsage, VkImage& image, VmaAllocation& allocation);

        // --- NEW GPU TRANSFER UTILITIES ---
        VkCommandBuffer BeginSingleTimeCommands();
        void EndSingleTimeCommands(VkCommandBuffer commandBuffer);
        void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
        void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
        void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
        VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

    private:
        VkInstance m_Instance;
        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
        VkDevice m_Device;
        VkSurfaceKHR m_Surface;
        VkQueue m_GraphicsQueue;
        VkQueue m_PresentQueue;
        VkCommandPool m_CommandPool;
        VmaAllocator m_Allocator = VK_NULL_HANDLE;

        const std::vector<const char*> m_DeviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        void CreateInstance();
        void CreateSurface(Window* window);
        void PickPhysicalDevice();
        void CreateLogicalDevice();
        void CreateCommandPool();
        void CreateAllocator();
        bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
    };
}
