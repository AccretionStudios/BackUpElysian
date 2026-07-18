#include "MeshRenderer.h"
#include "Core/Texture.h"
#include "VulkanTypes.h"
#include <stdexcept>
#include <cstring>
#include <array>
#include <glm/gtc/matrix_transform.hpp>

namespace Elysian
{
    void MeshRenderer::Init(VulkanContext* context, Scene* scene, int maxFramesInFlight)
    {
        m_Context = context;
        m_MaxFramesInFlight = maxFramesInFlight;

        CreateDefaultTexture(); // Generate our white pixel before the descriptors!
        CreateMeshDescriptorSetLayout();
        CreateLightDescriptorSet();
        CreateVertexAndIndexBuffers(scene);
        CreateUniformBuffers();
        CreateMeshDescriptorPoolAndSets();
    }

    void MeshRenderer::Cleanup()
    {
        VkDevice device = m_Context->GetDevice();

        m_DefaultTexture.Destroy(m_Context);
        m_DefaultNormal.Destroy(m_Context); // <--- Destroy normal map

        vkDestroyDescriptorPool(device, m_MeshDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, m_MeshDescriptorSetLayout, nullptr);
        vkDestroyDescriptorPool(device, m_LightDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, m_LightDescriptorSetLayout, nullptr);

        m_LightUniformBuffer.Destroy(m_Context);
        for (auto& ub : m_UniformBuffers) ub.Destroy(m_Context);

        m_IndexBuffer.Destroy(m_Context);
        m_VertexBuffer.Destroy(m_Context);
    }

    void MeshRenderer::CreateDefaultTexture()
    {
        // Helper lambda to load and create textures cleanly
        // FIX: outTex is now a GPUTexture&, not a Texture&
        auto loadTex = [&](const std::string& path, uint32_t fallbackColor, GPUTexture& outTex)
        {
            Texture tex;
            bool loaded = tex.LoadFromFile(path);

            VkDeviceSize imageSize;
            uint32_t width, height;
            Buffer stagingBuffer;

            if (loaded)
            {
                width = tex.GetWidth();
                height = tex.GetHeight();
                imageSize = width * height * tex.GetChannels();
                stagingBuffer.Create(m_Context, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
                stagingBuffer.Map(m_Context);
                memcpy(stagingBuffer.mapped, tex.GetPixels(), static_cast<size_t>(imageSize));
                stagingBuffer.Unmap(m_Context);
            }
            else
            {
                width = 1;
                height = 1;
                imageSize = 4;
                stagingBuffer.Create(m_Context, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
                stagingBuffer.Map(m_Context);
                memcpy(stagingBuffer.mapped, &fallbackColor, static_cast<size_t>(imageSize));
                stagingBuffer.Unmap(m_Context);
            }

            m_Context->CreateImageVMA(width, height, VK_FORMAT_R8G8B8A8_UNORM,
                                      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                      VMA_MEMORY_USAGE_GPU_ONLY, outTex.image, outTex.allocation);
            m_Context->TransitionImageLayout(outTex.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED,
                                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            m_Context->CopyBufferToImage(stagingBuffer.buffer, outTex.image, width, height);
            m_Context->TransitionImageLayout(outTex.image, VK_FORMAT_R8G8B8A8_UNORM,
                                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            stagingBuffer.Destroy(m_Context);
            outTex.view = m_Context->CreateImageView(outTex.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

            VkSamplerCreateInfo samplerInfo{};
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.anisotropyEnable = VK_TRUE;
            samplerInfo.maxAnisotropy = 16.0f;
            samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

            if (vkCreateSampler(m_Context->GetDevice(), &samplerInfo, nullptr, &outTex.sampler) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create texture sampler!");
            }
        };

        // Load both Albedo and Normal (0xFFFF8080 = Flat Normal Map Fallback)
        loadTex("../../../Assets/Textures/sofa_diffuse.png", 0xFFFFFFFF, m_DefaultTexture);
        loadTex("../../../Assets/Textures/sofa_normal.png", 0xFFFF8080, m_DefaultNormal);
    }

    void MeshRenderer::CreateMeshDescriptorSetLayout()
    {
        VkDescriptorSetLayoutBinding uboBinding{
            0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr
        };
        VkDescriptorSetLayoutBinding albedoBinding{
            1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr
        };
        VkDescriptorSetLayoutBinding normalBinding{
            2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr
        }; // <--- BINDING 2

        std::array<VkDescriptorSetLayoutBinding, 3> bindings = {uboBinding, albedoBinding, normalBinding};
        VkDescriptorSetLayoutCreateInfo info{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, static_cast<uint32_t>(bindings.size()),
            bindings.data()
        };

        if (vkCreateDescriptorSetLayout(m_Context->GetDevice(), &info, nullptr, &m_MeshDescriptorSetLayout) !=
            VK_SUCCESS)
            throw std::runtime_error("Failed to create mesh descriptor set layout");
    }

    void MeshRenderer::CreateVertexAndIndexBuffers(Scene* scene)
    {
        const auto& vertices = scene->m_Mesh.GetVertices();
        const auto& indices = scene->m_Mesh.GetIndices();
        m_IndexCount = static_cast<uint32_t>(indices.size());

        VkDeviceSize vSize = sizeof(Vertex) * vertices.size();
        Buffer vStaging;
        vStaging.Create(m_Context, vSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
        vStaging.Map(m_Context);
        memcpy(vStaging.mapped, vertices.data(), (size_t)vSize);
        vStaging.Unmap(m_Context);

        m_VertexBuffer.Create(m_Context, vSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                              VMA_MEMORY_USAGE_GPU_ONLY);
        m_Context->CopyBuffer(vStaging.buffer, m_VertexBuffer.buffer, vSize);
        vStaging.Destroy(m_Context);

        VkDeviceSize iSize = sizeof(uint32_t) * indices.size();
        Buffer iStaging;
        iStaging.Create(m_Context, iSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
        iStaging.Map(m_Context);
        memcpy(iStaging.mapped, indices.data(), (size_t)iSize);
        iStaging.Unmap(m_Context);

        m_IndexBuffer.Create(m_Context, iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                             VMA_MEMORY_USAGE_GPU_ONLY);
        m_Context->CopyBuffer(iStaging.buffer, m_IndexBuffer.buffer, iSize);
        iStaging.Destroy(m_Context);
    }

    void MeshRenderer::CreateUniformBuffers()
    {
        m_UniformBuffers.resize(m_MaxFramesInFlight);
        for (int i = 0; i < m_MaxFramesInFlight; i++)
        {
            m_UniformBuffers[i].Create(m_Context, sizeof(glm::mat4) * 2,
                                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                       VMA_MEMORY_USAGE_CPU_TO_GPU);
        }
    }

    void MeshRenderer::CreateMeshDescriptorPoolAndSets()
    {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(m_MaxFramesInFlight);
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(m_MaxFramesInFlight * 2);
        // <--- We now have 2 samplers per frame

        VkDescriptorPoolCreateInfo info{
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr, 0, (uint32_t)m_MaxFramesInFlight,
            static_cast<uint32_t>(poolSizes.size()), poolSizes.data()
        };
        if (vkCreateDescriptorPool(m_Context->GetDevice(), &info, nullptr, &m_MeshDescriptorPool) != VK_SUCCESS)
            throw std::runtime_error("Failed to create mesh descriptor pool");

        std::vector<VkDescriptorSetLayout> layouts(m_MaxFramesInFlight, m_MeshDescriptorSetLayout);
        VkDescriptorSetAllocateInfo alloc{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, m_MeshDescriptorPool,
            (uint32_t)m_MaxFramesInFlight, layouts.data()
        };
        m_MeshDescriptorSets.resize(m_MaxFramesInFlight);
        if (vkAllocateDescriptorSets(m_Context->GetDevice(), &alloc, m_MeshDescriptorSets.data()) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate mesh descriptor sets");

        for (int i = 0; i < m_MaxFramesInFlight; i++)
        {
            VkDescriptorBufferInfo bInfo{m_UniformBuffers[i].buffer, 0, sizeof(glm::mat4) * 2};

            VkDescriptorImageInfo albedoInfo{};
            albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            albedoInfo.imageView = m_DefaultTexture.view;
            albedoInfo.sampler = m_DefaultTexture.sampler;

            VkDescriptorImageInfo normalInfo{};
            normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            normalInfo.imageView = m_DefaultNormal.view;
            normalInfo.sampler = m_DefaultNormal.sampler;

            std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = m_MeshDescriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &bInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = m_MeshDescriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &albedoInfo;

            descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[2].dstSet = m_MeshDescriptorSets[i];
            descriptorWrites[2].dstBinding = 2; // <--- SLOT 2
            descriptorWrites[2].dstArrayElement = 0;
            descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pImageInfo = &normalInfo;

            vkUpdateDescriptorSets(m_Context->GetDevice(), static_cast<uint32_t>(descriptorWrites.size()),
                                   descriptorWrites.data(), 0, nullptr);
        }
    }

    void MeshRenderer::CreateLightDescriptorSet()
    {
        m_LightUniformBuffer.Create(m_Context, sizeof(LightUBO),
                                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                    VMA_MEMORY_USAGE_CPU_TO_GPU);

        VkDescriptorSetLayoutBinding lightBinding = {
            0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr
        };
        VkDescriptorSetLayoutCreateInfo layoutInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 1, &lightBinding
        };
        if (vkCreateDescriptorSetLayout(m_Context->GetDevice(), &layoutInfo, nullptr, &m_LightDescriptorSetLayout) !=
            VK_SUCCESS)
            throw std::runtime_error("Failed to create light descriptor set layout");

        VkDescriptorPoolSize poolSize = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1};
        VkDescriptorPoolCreateInfo poolInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr, 0, 1, 1, &poolSize
        };
        if (vkCreateDescriptorPool(m_Context->GetDevice(), &poolInfo, nullptr, &m_LightDescriptorPool) != VK_SUCCESS)
            throw std::runtime_error("Failed to create light descriptor pool");

        VkDescriptorSetAllocateInfo allocInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, m_LightDescriptorPool, 1,
            &m_LightDescriptorSetLayout
        };
        if (vkAllocateDescriptorSets(m_Context->GetDevice(), &allocInfo, &m_LightDescriptorSet) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate light descriptor set");

        VkDescriptorBufferInfo bufferInfo = {m_LightUniformBuffer.buffer, 0, sizeof(LightUBO)};
        VkWriteDescriptorSet write = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_LightDescriptorSet, 0, 0, 1,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &bufferInfo, nullptr
        };
        vkUpdateDescriptorSets(m_Context->GetDevice(), 1, &write, 0, nullptr);
    }

    void MeshRenderer::UpdateViewProjUniformBuffer(uint32_t frameIndex, const glm::mat4& view, const glm::mat4& proj)
    {
        struct ViewProj
        {
            glm::mat4 view;
            glm::mat4 proj;
        } data;
        data.view = view;
        data.proj = proj;
        memcpy(m_UniformBuffers[frameIndex].mapped, &data, sizeof(data));
    }

    void MeshRenderer::UpdateLightUniformBuffer(const LightUBO& lightData)
    {
        memcpy(m_LightUniformBuffer.mapped, &lightData, sizeof(lightData));
    }
}
