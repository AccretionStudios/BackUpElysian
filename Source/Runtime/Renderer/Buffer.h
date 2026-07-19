#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "VMA/vk_mem_alloc.h"
#include "Core/VulkanContext.h"
#include <stdexcept>

namespace Elysian
{
    class Buffer
    {
    public:
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        void* mapped = nullptr;
        VkDeviceSize size = 0;

        Buffer() = default;

        void Create(VulkanContext* context, VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
        {
            this->size = size;

            VkBufferCreateInfo bufferInfo = {};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = size;
            bufferInfo.usage = usage;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = memoryUsage;
            if (memoryUsage == VMA_MEMORY_USAGE_CPU_TO_GPU)
            {
                allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
            }

            VmaAllocator allocator = context->GetAllocator();
            VmaAllocationInfo allocInfoOut;
            if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, &allocInfoOut) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create buffer with VMA");
            }

            if (memoryUsage == VMA_MEMORY_USAGE_CPU_TO_GPU)
            {
                mapped = allocInfoOut.pMappedData;
            }
            else
            {
                mapped = nullptr;
            }
        }

        void Map(VulkanContext* context)
        {
            VkResult result = vmaMapMemory(context->GetAllocator(), allocation, &mapped);
            if (result != VK_SUCCESS || mapped == nullptr) {
                throw std::runtime_error("Failed to map buffer memory");
            }
        }

        void Unmap(VulkanContext* context)
        {
            if (mapped)
            {
                vmaUnmapMemory(context->GetAllocator(), allocation);
                mapped = nullptr;
            }
        }

        void Destroy(VulkanContext* context)
        {
            if (buffer)
            {
                // Do NOT unmap here – VMA will handle unmapping during destruction.
                vmaDestroyBuffer(context->GetAllocator(), buffer, allocation);
                buffer = VK_NULL_HANDLE;
                allocation = VK_NULL_HANDLE;
                mapped = nullptr;
            }
        }
    };
}
