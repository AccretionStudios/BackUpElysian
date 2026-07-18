#pragma once
#include "Core/VulkanContext.h"
#include "Scene/Scene.h"
#include "Buffer.h"
#include "VulkanTypes.h"
#include <vector>

namespace Elysian
{
    // Helper struct to manage GPU texture memory
    struct GPUTexture
    {
        VkImage image = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;

        void Destroy(VulkanContext* context)
        {
            VkDevice device = context->GetDevice();
            if (sampler) vkDestroySampler(device, sampler, nullptr);
            if (view) vkDestroyImageView(device, view, nullptr);
            if (image) vmaDestroyImage(context->GetAllocator(), image, allocation);
        }
    };

    class MeshRenderer
    {
    public:
        void Init(VulkanContext* context, Scene* scene, int maxFramesInFlight);
        void Cleanup();

        void UpdateViewProjUniformBuffer(uint32_t frameIndex, const glm::mat4& view, const glm::mat4& proj);
        void UpdateLightUniformBuffer(uint32_t frameIndex, const LightUBO& lightData);

        VkDescriptorSetLayout GetMeshDescriptorLayout() const { return m_MeshDescriptorSetLayout; }
        VkDescriptorSetLayout GetLightDescriptorLayout() const { return m_LightDescriptorSetLayout; }
        VkDescriptorSet GetMeshDescriptorSet(uint32_t frameIndex) const { return m_MeshDescriptorSets[frameIndex]; }
        VkDescriptorSet GetLightDescriptorSet(uint32_t frameIndex) const { return m_LightDescriptorSets[frameIndex]; }

        VkBuffer GetVertexBuffer() const { return m_VertexBuffer.buffer; }
        VkBuffer GetIndexBuffer() const { return m_IndexBuffer.buffer; }
        uint32_t GetIndexCount() const { return m_IndexCount; }
        
        void UpdateModelBuffer(uint32_t frameIndex, const std::vector<glm::mat4>& models);

    private:
        VulkanContext* m_Context = nullptr;
        int m_MaxFramesInFlight = 2;

        Buffer m_VertexBuffer;
        Buffer m_IndexBuffer;
        uint32_t m_IndexCount = 0;

        std::vector<Buffer> m_UniformBuffers;
        std::vector<Buffer> m_LightUniformBuffers;
        std::vector<Buffer> m_ModelBuffers;
        static constexpr uint32_t MAX_ENTITIES = 2048;

        VkDescriptorSetLayout m_MeshDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool m_MeshDescriptorPool = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> m_MeshDescriptorSets;

        VkDescriptorSetLayout m_LightDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool m_LightDescriptorPool = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> m_LightDescriptorSets;

        GPUTexture m_DefaultTexture;
        GPUTexture m_DefaultNormal;

        void CreateDefaultTexture();
        void CreateMeshDescriptorSetLayout();
        void CreateVertexAndIndexBuffers(Scene* scene);
        void CreateUniformBuffers();
        void CreateMeshDescriptorPoolAndSets();
        void CreateLightDescriptorSet();
    };
}
