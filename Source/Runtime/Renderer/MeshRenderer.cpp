#include "MeshRenderer.h"
#include "Core/Texture.h"
#include "Core/AssetManager.h"
#include "VulkanTypes.h"
#include <stdexcept>
#include <cstring>
#include <array>
#include <iostream>
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
        BuildMegaBuffers(scene);
        CreateUniformBuffers();
        CreateMeshDescriptorPoolAndSets();
    }

    void MeshRenderer::Cleanup()
    {
        VkDevice device = m_Context->GetDevice();

        m_DefaultTexture.Destroy(m_Context);
        m_DefaultNormal.Destroy(m_Context);

        vkDestroyDescriptorPool(device, m_MeshDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, m_MeshDescriptorSetLayout, nullptr);
        vkDestroyDescriptorPool(device, m_LightDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, m_LightDescriptorSetLayout, nullptr);

        for (auto& ub : m_LightUniformBuffers) ub.Destroy(m_Context);
        for (auto& ub : m_UniformBuffers) ub.Destroy(m_Context);
        for (auto& mb : m_ModelBuffers) mb.Destroy(m_Context);

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
        VkDescriptorSetLayoutBinding uboBinding{};
        uboBinding.binding = 0;
        uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboBinding.descriptorCount = 1;
        uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding albedoBinding{};
        albedoBinding.binding = 1;
        albedoBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        albedoBinding.descriptorCount = 1;
        albedoBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding normalBinding{};
        normalBinding.binding = 2;
        normalBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        normalBinding.descriptorCount = 1;
        normalBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        // Storage Buffer for Model Matrices
        VkDescriptorSetLayoutBinding modelBufferBinding{};
        modelBufferBinding.binding = 3;
        modelBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; // Use Storage for large arrays
        modelBufferBinding.descriptorCount = 1;
        modelBufferBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        std::array<VkDescriptorSetLayoutBinding, 4> bindings = {
            uboBinding, albedoBinding, normalBinding, modelBufferBinding
        };

        VkDescriptorSetLayoutCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = static_cast<uint32_t>(bindings.size());
        info.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(m_Context->GetDevice(), &info, nullptr, &m_MeshDescriptorSetLayout) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create mesh descriptor set layout");
        }
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
        // Pool sizes: UBO (1 per frame) + 2 Samplers (albedo+normal) + 1 Storage Buffer (model)
        std::array<VkDescriptorPoolSize, 3> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(m_MaxFramesInFlight);
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(m_MaxFramesInFlight * 2); // albedo + normal
        poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[2].descriptorCount = static_cast<uint32_t>(m_MaxFramesInFlight);

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = static_cast<uint32_t>(m_MaxFramesInFlight);
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();

        if (vkCreateDescriptorPool(m_Context->GetDevice(), &poolInfo, nullptr, &m_MeshDescriptorPool) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create mesh descriptor pool");
        }

        // --- Create the model buffers (one per frame) ---
        m_ModelBuffers.resize(m_MaxFramesInFlight);
        VkDeviceSize bufferSize = MAX_ENTITIES * sizeof(glm::mat4);
        for (int i = 0; i < m_MaxFramesInFlight; i++)
        {
            m_ModelBuffers[i].Create(m_Context, bufferSize,
                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                     VMA_MEMORY_USAGE_CPU_TO_GPU);
        }

        // Allocate descriptor sets (same as before, just with an extra write)
        m_MeshDescriptorSets.resize(m_MaxFramesInFlight);
        std::vector<VkDescriptorSetLayout> layouts(m_MaxFramesInFlight, m_MeshDescriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_MeshDescriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(m_MaxFramesInFlight);
        allocInfo.pSetLayouts = layouts.data();

        if (vkAllocateDescriptorSets(m_Context->GetDevice(), &allocInfo, m_MeshDescriptorSets.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate mesh descriptor sets");
        }

        // Update descriptors per frame
        for (int i = 0; i < m_MaxFramesInFlight; i++)
        {
            // UBO buffer info
            VkDescriptorBufferInfo uboInfo{};
            uboInfo.buffer = m_UniformBuffers[i].buffer;
            uboInfo.offset = 0;
            uboInfo.range = sizeof(glm::mat4) * 2; // view + proj

            // Albedo image info
            VkDescriptorImageInfo albedoInfo{};
            albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            albedoInfo.imageView = m_DefaultTexture.view;
            albedoInfo.sampler = m_DefaultTexture.sampler;

            // Normal image info
            VkDescriptorImageInfo normalInfo{};
            normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            normalInfo.imageView = m_DefaultNormal.view;
            normalInfo.sampler = m_DefaultNormal.sampler;

            // --- NEW: Model buffer info ---
            VkDescriptorBufferInfo modelBufferInfo{};
            modelBufferInfo.buffer = m_ModelBuffers[i].buffer;
            modelBufferInfo.offset = 0;
            modelBufferInfo.range = MAX_ENTITIES * sizeof(glm::mat4);

            std::array<VkWriteDescriptorSet, 4> descriptorWrites{};
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = m_MeshDescriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &uboInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = m_MeshDescriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &albedoInfo;

            descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[2].dstSet = m_MeshDescriptorSets[i];
            descriptorWrites[2].dstBinding = 2;
            descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pImageInfo = &normalInfo;

            // --- NEW: Write model buffer binding ---
            descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[3].dstSet = m_MeshDescriptorSets[i];
            descriptorWrites[3].dstBinding = 3;
            descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[3].descriptorCount = 1;
            descriptorWrites[3].pBufferInfo = &modelBufferInfo;

            vkUpdateDescriptorSets(m_Context->GetDevice(),
                                   static_cast<uint32_t>(descriptorWrites.size()),
                                   descriptorWrites.data(), 0, nullptr);
        }
    }

    void MeshRenderer::CreateLightDescriptorSet()
    {
        // Create per-frame uniform buffers
        m_LightUniformBuffers.resize(m_MaxFramesInFlight);
        for (int i = 0; i < m_MaxFramesInFlight; i++)
        {
            m_LightUniformBuffers[i].Create(m_Context, sizeof(LightUBO),
                                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                            VMA_MEMORY_USAGE_CPU_TO_GPU);
        }

        // Create descriptor set layout
        VkDescriptorSetLayoutBinding lightBinding{};
        lightBinding.binding = 0;
        lightBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        lightBinding.descriptorCount = 1;
        lightBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &lightBinding;

        VkResult layoutResult = vkCreateDescriptorSetLayout(m_Context->GetDevice(), &layoutInfo, nullptr,
                                                            &m_LightDescriptorSetLayout);
        if (layoutResult != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create light descriptor set layout: " + std::to_string(layoutResult));
        }

        // Create a descriptor pool that can hold enough sets
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = m_MaxFramesInFlight; // one per frame

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = m_MaxFramesInFlight;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;

        VkResult poolResult =
            vkCreateDescriptorPool(m_Context->GetDevice(), &poolInfo, nullptr, &m_LightDescriptorPool);
        if (poolResult != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create light descriptor pool: " + std::to_string(poolResult));
        }

        // Resize the vector and allocate per-frame descriptor sets
        m_LightDescriptorSets.resize(m_MaxFramesInFlight); // <-- CRITICAL

        for (int i = 0; i < m_MaxFramesInFlight; i++)
        {
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = m_LightDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &m_LightDescriptorSetLayout;

            VkResult result = vkAllocateDescriptorSets(m_Context->GetDevice(), &allocInfo, &m_LightDescriptorSets[i]);
            if (result != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to allocate light descriptor set for frame " + std::to_string(i) +
                    " (VkResult: " + std::to_string(result) + ")");
            }

            // Update the descriptor set with the buffer for this specific frame
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = m_LightUniformBuffers[i].buffer;
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(LightUBO);

            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = m_LightDescriptorSets[i];
            write.dstBinding = 0;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.pBufferInfo = &bufferInfo;

            vkUpdateDescriptorSets(m_Context->GetDevice(), 1, &write, 0, nullptr);
        }
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

    void MeshRenderer::UpdateLightUniformBuffer(uint32_t frameIndex, const LightUBO& lightData)
    {
        void* data = m_LightUniformBuffers[frameIndex].mapped;
        memcpy(data, &lightData, sizeof(LightUBO));
    }

    void MeshRenderer::UpdateModelBuffer(uint32_t frameIndex, const std::vector<glm::mat4>& models)
    {
        if (models.size() > MAX_ENTITIES)
        {
            throw std::runtime_error("Too many entities for model buffer! Increase MAX_ENTITIES.");
        }

        void* data = m_ModelBuffers[frameIndex].mapped;
        memcpy(data, models.data(), models.size() * sizeof(glm::mat4));
    }

    void MeshRenderer::BuildMegaBuffers(Scene* scene)
    {
        // 1. Iterate ALL entities with a MeshComponent and collect their data
        std::vector<Vertex> allVertices;
        std::vector<uint32_t> allIndices;
        
        auto& assetMgr = AssetManager::Get();
        auto view = scene->m_ECSManager.GetRegistry().view<MeshComponent>();
        for (auto entity : view)
        {
            auto& meshComp = view.get<MeshComponent>(entity);
            Mesh* mesh = assetMgr.GetMesh(meshComp.meshHandle);
            if (!mesh) {
                std::cerr << "Warning: Entity has invalid mesh handle!" << std::endl;
                continue;
            }

            // Store offsets BEFORE we append
            meshComp.vertexOffset = static_cast<uint32_t>(allVertices.size());
            meshComp.indexOffset = static_cast<uint32_t>(allIndices.size());
            meshComp.indexCount = static_cast<uint32_t>(mesh->GetIndices().size());

            // Append raw vertex data
            const auto& vertices = mesh->GetVertices();
            allVertices.insert(allVertices.end(), vertices.begin(), vertices.end());

            // Append rebased indices
            for (uint32_t idx : mesh->GetIndices()) {
                allIndices.push_back(idx + meshComp.vertexOffset);
            }
        }

        // 2. If there are NO meshes, just bail out (or create a dummy triangle)
        if (allVertices.empty())
        {
            std::cerr << "Warning: No meshes found in scene!" << std::endl;
            return;
        }

        // 3. Create the VERTEX buffer
        VkDeviceSize vSize = sizeof(Vertex) * allVertices.size();
        Buffer vStaging;
        vStaging.Create(m_Context, vSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
        vStaging.Map(m_Context);
        memcpy(vStaging.mapped, allVertices.data(), (size_t)vSize);
        vStaging.Unmap(m_Context);

        m_VertexBuffer.Create(m_Context, vSize,
                              VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                              VMA_MEMORY_USAGE_GPU_ONLY);
        m_Context->CopyBuffer(vStaging.buffer, m_VertexBuffer.buffer, vSize);
        vStaging.Destroy(m_Context);

        // 4. Create the INDEX buffer
        VkDeviceSize iSize = sizeof(uint32_t) * allIndices.size();
        Buffer iStaging;
        iStaging.Create(m_Context, iSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
        iStaging.Map(m_Context);
        memcpy(iStaging.mapped, allIndices.data(), (size_t)iSize);
        iStaging.Unmap(m_Context);

        m_IndexBuffer.Create(m_Context, iSize,
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                             VMA_MEMORY_USAGE_GPU_ONLY);
        m_Context->CopyBuffer(iStaging.buffer, m_IndexBuffer.buffer, iSize);
        iStaging.Destroy(m_Context);

        // 5. Store total count for safety (not strictly needed for drawing anymore)
        m_IndexCount = static_cast<uint32_t>(allIndices.size());

        std::cout << "[MeshRenderer] Merged " << allVertices.size() << " vertices and "
            << allIndices.size() << " indices from " << view.size() << " meshes." << std::endl;
    }
}
