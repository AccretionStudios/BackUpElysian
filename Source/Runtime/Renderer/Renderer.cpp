#include "Renderer.h"
#include "Renderer/Pipeline.h"
#include "ECS/Components.h"
#include "VulkanTypes.h"
#include <stdexcept>
#include <array>

namespace Elysian
{
    void Renderer::Init(VulkanContext* context, Swapchain* swapchain, Scene* scene)
    {
        m_Context = context;
        m_Swapchain = swapchain;

        m_MeshRenderer.Init(m_Context, scene, MAX_FRAMES_IN_FLIGHT);

        GBufferCreateInfo gbInfo = {};
        gbInfo.width = m_Swapchain->GetExtent().width;
        gbInfo.height = m_Swapchain->GetExtent().height;
        m_GBuffer.Init(m_Context, gbInfo);

        CreateHdrResources();

        m_GeometryPass.Init(m_Context, &m_GBuffer, m_MeshRenderer.GetMeshDescriptorLayout());
        m_LightingPass.Init(m_Context, &m_GBuffer, m_HdrRenderPass);

        CreatePresentPipeline();
        CreateGBufferDescriptorSet();
        CreateGridPipeline();
        CreateCommandBuffers();
        CreateSyncObjects();
    }

    void Renderer::Cleanup()
    {
        m_MeshRenderer.Cleanup();
        m_GBuffer.Cleanup();
        m_GeometryPass.Cleanup();
        m_LightingPass.Cleanup();
        DestroyHdrResources();
        DestroyPresentPipeline();

        VkDevice device = m_Context->GetDevice();
        
        if (m_GridPipeline) {
            vkDestroyPipeline(device, m_GridPipeline, nullptr);
        }
        if (m_GridPipelineLayout) {
            vkDestroyPipelineLayout(device, m_GridPipelineLayout, nullptr);
        }
        
        vkDestroyDescriptorPool(device, m_LightingDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, m_GBufferDescriptorSetLayout, nullptr);
        if (m_GBufferSampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(device, m_GBufferSampler, nullptr);
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(device, m_RenderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device, m_ImageAvailableSemaphores[i], nullptr);
            vkDestroyFence(device, m_InFlightFences[i], nullptr);
        }
    }

    void Renderer::RecreateResolutionDependentResources(Swapchain* swapchain)
    {
        vkDeviceWaitIdle(m_Context->GetDevice());

        DestroyHdrResources();
        DestroyPresentPipeline();

        m_GBuffer.Cleanup();
        GBufferCreateInfo gbInfo = {};
        gbInfo.width = swapchain->GetExtent().width;
        gbInfo.height = swapchain->GetExtent().height;
        m_GBuffer.Init(m_Context, gbInfo);

        CreateHdrResources();
        CreatePresentPipeline();
        CreateGBufferDescriptorSet();
    }

    bool Renderer::DrawFrame(Scene* scene, DebugUI* ui, Swapchain* swapchain)
    {
        vkWaitForFences(m_Context->GetDevice(), 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imgIdx;
        VkResult res = vkAcquireNextImageKHR(m_Context->GetDevice(), swapchain->GetSwapchain(), UINT64_MAX,
                                             m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &imgIdx);
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) return false;

        vkResetFences(m_Context->GetDevice(), 1, &m_InFlightFences[m_CurrentFrame]);

        ui->BeginFrame();
        ui->DrawWindows(scene, swapchain, 0.016f);

        // ========== BUILD LIGHT UBO FROM ECS ==========
        LightUBO lightUBO = {};
        lightUBO.ambientStrength = scene->m_AmbientStrength;

        auto lightView = scene->m_ECSManager.GetRegistry().view<TransformComponent, LightComponent>();
        int dirCount = 0, pointCount = 0, spotCount = 0;

        for (auto entity : lightView)
        {
            auto& transform = lightView.get<TransformComponent>(entity);
            auto& light = lightView.get<LightComponent>(entity);

            switch (light.type)
            {
            case LightType::Directional:
                {
                    if (dirCount == 0)
                    {
                        // only first directional light used
                        // direction = forward vector from rotation
                        glm::mat4 rotMat = glm::rotate(glm::mat4(1.0f), glm::radians(transform.Rotation.y),
                                                       glm::vec3(0, 1, 0));
                        rotMat = glm::rotate(rotMat, glm::radians(transform.Rotation.x), glm::vec3(1, 0, 0));
                        rotMat = glm::rotate(rotMat, glm::radians(transform.Rotation.z), glm::vec3(0, 0, 1));
                        glm::vec3 forward = rotMat * glm::vec4(0, 0, -1, 0);
                        lightUBO.dirLightDirection = forward;
                        lightUBO.dirLightIntensity = light.intensity;
                        lightUBO.dirLightColor = light.color;
                    }
                    dirCount++;
                    break;
                }
            case LightType::Point:
                {
                    if (pointCount < 4)
                    {
                        lightUBO.pointLights[pointCount].position = transform.Position;
                        lightUBO.pointLights[pointCount].radius = light.radius;
                        lightUBO.pointLights[pointCount].color = light.color;
                        lightUBO.pointLights[pointCount].intensity = light.intensity;
                        pointCount++;
                    }
                    break;
                }
            case LightType::Spot:
                {
                    if (spotCount < 4)
                    {
                        // direction from rotation
                        glm::mat4 rotMat = glm::rotate(glm::mat4(1.0f), glm::radians(transform.Rotation.y),
                                                       glm::vec3(0, 1, 0));
                        rotMat = glm::rotate(rotMat, glm::radians(transform.Rotation.x), glm::vec3(1, 0, 0));
                        rotMat = glm::rotate(rotMat, glm::radians(transform.Rotation.z), glm::vec3(0, 0, 1));
                        glm::vec3 forward = rotMat * glm::vec4(0, 0, -1, 0);
                        lightUBO.spotLights[spotCount].position = transform.Position;
                        lightUBO.spotLights[spotCount].radius = light.radius;
                        lightUBO.spotLights[spotCount].direction = forward;
                        lightUBO.spotLights[spotCount].innerAngle = light.innerAngle;
                        lightUBO.spotLights[spotCount].outerAngle = light.outerAngle;
                        lightUBO.spotLights[spotCount].color = light.color;
                        lightUBO.spotLights[spotCount].intensity = light.intensity;
                        spotCount++;
                    }
                    break;
                }
            }
        }
        lightUBO.numPointLights = pointCount;
        lightUBO.numSpotLights = spotCount;
        
        lightUBO.cameraPos = scene->m_Camera.Position;
        
        // Update light uniform buffer
        m_MeshRenderer.UpdateLightUniformBuffer(lightUBO);

        // Update view and proj matrices
        glm::mat4 viewMat = scene->m_Camera.GetViewMatrix();
        glm::mat4 projMat = scene->m_Camera.GetProjectionMatrix(
            (float)swapchain->GetExtent().width / (float)swapchain->GetExtent().height);
        m_MeshRenderer.UpdateViewProjUniformBuffer(m_CurrentFrame, viewMat, projMat);

        vkResetCommandBuffer(m_CommandBuffers[m_CurrentFrame], 0);
        VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkBeginCommandBuffer(m_CommandBuffers[m_CurrentFrame], &beginInfo);

        // ========== Geometry pass ==========
        {
            std::array<VkImageView, 4> attachments = {
                m_GBuffer.GetImageViews()[0], m_GBuffer.GetImageViews()[1],
                m_GBuffer.GetImageViews()[2], m_GBuffer.GetDepthImageView()
            };
            VkFramebufferCreateInfo fbInfo = {
                VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, nullptr, 0, m_GBuffer.GetRenderPass(), 4, attachments.data(),
                m_GBuffer.GetExtent().width, m_GBuffer.GetExtent().height, 1
            };
            VkFramebuffer gbufferFramebuffer;
            vkCreateFramebuffer(m_Context->GetDevice(), &fbInfo, nullptr, &gbufferFramebuffer);

            VkRenderPassBeginInfo rpInfo = {
                VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr, m_GBuffer.GetRenderPass(), gbufferFramebuffer,
                {{0, 0}, m_GBuffer.GetExtent()}
            };
            std::array<VkClearValue, 4> clearValues = {};
            clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            clearValues[1].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            clearValues[2].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            clearValues[3].depthStencil = {1.0f, 0};
            rpInfo.clearValueCount = 4;
            rpInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(m_CommandBuffers[m_CurrentFrame], &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport{
                0.0f, 0.0f, (float)m_GBuffer.GetExtent().width, (float)m_GBuffer.GetExtent().height, 0.0f, 1.0f
            };
            vkCmdSetViewport(m_CommandBuffers[m_CurrentFrame], 0, 1, &viewport);
            VkRect2D scissor{{0, 0}, m_GBuffer.GetExtent()};
            vkCmdSetScissor(m_CommandBuffers[m_CurrentFrame], 0, 1, &scissor);

            auto view = scene->m_ECSManager.GetRegistry().view<TransformComponent, MeshComponent>();
            for (auto entity : view)
            {
                auto& transform = view.get<TransformComponent>(entity);
                auto& meshComp = view.get<MeshComponent>(entity);
                if (!meshComp.mesh) continue;

                glm::mat4 model = transform.GetModelMatrix();
                float determinant = transform.Scale.x * transform.Scale.y * transform.Scale.z;
                VkFrontFace frontFace = (determinant < 0.0f)
                                            ? VK_FRONT_FACE_CLOCKWISE
                                            : VK_FRONT_FACE_COUNTER_CLOCKWISE;

                m_GeometryPass.Record(m_CommandBuffers[m_CurrentFrame], m_CurrentFrame,
                                      m_MeshRenderer.GetMeshDescriptorSet(m_CurrentFrame),
                                      m_MeshRenderer.GetVertexBuffer(), m_MeshRenderer.GetIndexBuffer(),
                                      m_MeshRenderer.GetIndexCount(), model, frontFace);
            }
            
            vkCmdBindPipeline(m_CommandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, m_GridPipeline);

            struct GridPushConstants {
                glm::mat4 view;
                glm::mat4 proj;
            } pcs;
            
            pcs.view = scene->m_Camera.GetViewMatrix();
            pcs.proj = scene->m_Camera.GetProjectionMatrix((float)m_GBuffer.GetExtent().width / (float)m_GBuffer.GetExtent().height);
            
            vkCmdPushConstants(m_CommandBuffers[m_CurrentFrame], m_GridPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GridPushConstants), &pcs);
            
            // Draw 1 Quad (6 Vertices) procedurally
            vkCmdDraw(m_CommandBuffers[m_CurrentFrame], 6, 1, 0, 0);
            // ----------------------------------------------------------------

            vkCmdEndRenderPass(m_CommandBuffers[m_CurrentFrame]);
            vkDestroyFramebuffer(m_Context->GetDevice(), gbufferFramebuffer, nullptr);
        }

        // ========== Lighting pass ==========
        {
            VkRenderPassBeginInfo rpInfo = {
                VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr, m_HdrRenderPass, m_HdrFramebuffer,
                {{0, 0}, swapchain->GetExtent()}
            };
            VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
            rpInfo.clearValueCount = 1;
            rpInfo.pClearValues = &clearColor;

            vkCmdBeginRenderPass(m_CommandBuffers[m_CurrentFrame], &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport{
                0.0f, 0.0f, (float)swapchain->GetExtent().width, (float)swapchain->GetExtent().height, 0.0f, 1.0f
            };
            vkCmdSetViewport(m_CommandBuffers[m_CurrentFrame], 0, 1, &viewport);
            VkRect2D scissor{{0, 0}, swapchain->GetExtent()};
            vkCmdSetScissor(m_CommandBuffers[m_CurrentFrame], 0, 1, &scissor);

            vkCmdBindDescriptorSets(m_CommandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_LightingPass.GetPipelineLayout(), 0, 1, &m_LightingDescriptorSet, 0, nullptr);
            VkDescriptorSet lightSet = m_MeshRenderer.GetLightDescriptorSet();
            vkCmdBindDescriptorSets(m_CommandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_LightingPass.GetPipelineLayout(), 1, 1, &lightSet, 0, nullptr);

            m_LightingPass.Record(m_CommandBuffers[m_CurrentFrame]);
            vkCmdEndRenderPass(m_CommandBuffers[m_CurrentFrame]);
        }

        // ========== Present pass ==========
        {
            VkRenderPassBeginInfo rpInfo = {
                VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr, swapchain->GetRenderPass(),
                swapchain->GetFramebuffer(imgIdx), {{0, 0}, swapchain->GetExtent()}
            };
            std::array<VkClearValue, 2> clear = {};
            clear[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            clear[1].depthStencil = {1.0f, 0};
            rpInfo.clearValueCount = 2;
            rpInfo.pClearValues = clear.data();

            vkCmdBeginRenderPass(m_CommandBuffers[m_CurrentFrame], &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport{
                0.0f, 0.0f, (float)swapchain->GetExtent().width, (float)swapchain->GetExtent().height, 0.0f, 1.0f
            };
            vkCmdSetViewport(m_CommandBuffers[m_CurrentFrame], 0, 1, &viewport);
            VkRect2D scissor{{0, 0}, swapchain->GetExtent()};
            vkCmdSetScissor(m_CommandBuffers[m_CurrentFrame], 0, 1, &scissor);

            vkCmdBindDescriptorSets(m_CommandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_PresentPipelineLayout, 0, 1, &m_PresentDescSet, 0, nullptr);
            vkCmdBindPipeline(m_CommandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, m_PresentPipeline);
            vkCmdDraw(m_CommandBuffers[m_CurrentFrame], 3, 1, 0, 0);

            ui->Render(m_CommandBuffers[m_CurrentFrame]);
            vkCmdEndRenderPass(m_CommandBuffers[m_CurrentFrame]);
        }

        vkEndCommandBuffer(m_CommandBuffers[m_CurrentFrame]);

        VkSubmitInfo sSubmit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        VkSemaphore waitSems[] = {m_ImageAvailableSemaphores[m_CurrentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        sSubmit.waitSemaphoreCount = 1;
        sSubmit.pWaitSemaphores = waitSems;
        sSubmit.pWaitDstStageMask = waitStages;
        sSubmit.commandBufferCount = 1;
        sSubmit.pCommandBuffers = &m_CommandBuffers[m_CurrentFrame];
        VkSemaphore sigSems[] = {m_RenderFinishedSemaphores[m_CurrentFrame]};
        sSubmit.signalSemaphoreCount = 1;
        sSubmit.pSignalSemaphores = sigSems;
        vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &sSubmit, m_InFlightFences[m_CurrentFrame]);

        VkPresentInfoKHR pInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        pInfo.waitSemaphoreCount = 1;
        pInfo.pWaitSemaphores = sigSems;
        VkSwapchainKHR swapChains[] = {swapchain->GetSwapchain()};
        pInfo.swapchainCount = 1;
        pInfo.pSwapchains = swapChains;
        pInfo.pImageIndices = &imgIdx;
        res = vkQueuePresentKHR(m_Context->GetPresentQueue(), &pInfo);

        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) return false;

        m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        return true;
    }

    void Renderer::CreateGBufferDescriptorSet()
    {
        if (m_GBufferDescriptorSetLayout == VK_NULL_HANDLE)
        {
            VkDescriptorSetLayoutBinding bindings[3] = {};
            for (int i = 0; i < 3; ++i)
            {
                bindings[i].binding = i;
                bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                bindings[i].descriptorCount = 1;
                bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            }
            VkDescriptorSetLayoutCreateInfo layoutInfo = {};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = 3;
            layoutInfo.pBindings = bindings;
            if (vkCreateDescriptorSetLayout(m_Context->GetDevice(), &layoutInfo, nullptr, &m_GBufferDescriptorSetLayout)
                != VK_SUCCESS)
                throw std::runtime_error("Failed to create G-buffer descriptor set layout");
        }

        if (m_LightingDescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(m_Context->GetDevice(), m_LightingDescriptorPool, nullptr);
            m_LightingDescriptorPool = VK_NULL_HANDLE;
        }

        VkDescriptorPoolSize poolSize = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3};
        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        if (vkCreateDescriptorPool(m_Context->GetDevice(), &poolInfo, nullptr, &m_LightingDescriptorPool) != VK_SUCCESS)
            throw std::runtime_error("Failed to create lighting descriptor pool");

        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_LightingDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_GBufferDescriptorSetLayout;
        if (vkAllocateDescriptorSets(m_Context->GetDevice(), &allocInfo, &m_LightingDescriptorSet) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate lighting descriptor set");

        if (m_GBufferSampler == VK_NULL_HANDLE)
        {
            VkSamplerCreateInfo samplerInfo = {};
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
            if (vkCreateSampler(m_Context->GetDevice(), &samplerInfo, nullptr, &m_GBufferSampler) != VK_SUCCESS)
                throw std::runtime_error("Failed to create GBuffer sampler");
        }

        std::array<VkDescriptorImageInfo, 3> imageInfos = {
            m_GBuffer.GetAlbedoDescriptor(m_GBufferSampler),
            m_GBuffer.GetNormalDescriptor(m_GBufferSampler),
            m_GBuffer.GetWorldPosDescriptor(m_GBufferSampler)
        };

        std::array<VkWriteDescriptorSet, 3> writes = {};
        for (int i = 0; i < 3; ++i)
        {
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = m_LightingDescriptorSet;
            writes[i].dstBinding = i;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[i].pImageInfo = &imageInfos[i];
        }
        vkUpdateDescriptorSets(m_Context->GetDevice(), 3, writes.data(), 0, nullptr);
    }

    void Renderer::CreateHdrResources()
    {
        VkExtent2D extent = m_Swapchain->GetExtent();
        VkFormat hdrFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        m_Context->CreateImageVMA(extent.width, extent.height, hdrFormat, usage, VMA_MEMORY_USAGE_GPU_ONLY, m_HdrImage,
                                  m_HdrAllocation);

        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_HdrImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = hdrFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(m_Context->GetDevice(), &viewInfo, nullptr, &m_HdrImageView) != VK_SUCCESS)
            throw std::runtime_error("Failed to create HDR image view");

        VkAttachmentDescription hdrAtt = {};
        hdrAtt.format = hdrFormat;
        hdrAtt.samples = VK_SAMPLE_COUNT_1_BIT;
        hdrAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        hdrAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        hdrAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        hdrAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        hdrAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        hdrAtt.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference hdrColorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription hdrSubpass = {};
        hdrSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        hdrSubpass.colorAttachmentCount = 1;
        hdrSubpass.pColorAttachments = &hdrColorRef;

        VkRenderPassCreateInfo rpInfo = {};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.attachmentCount = 1;
        rpInfo.pAttachments = &hdrAtt;
        rpInfo.subpassCount = 1;
        rpInfo.pSubpasses = &hdrSubpass;
        if (vkCreateRenderPass(m_Context->GetDevice(), &rpInfo, nullptr, &m_HdrRenderPass) != VK_SUCCESS)
            throw std::runtime_error("Failed to create HDR render pass");

        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = m_HdrRenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &m_HdrImageView;
        fbInfo.width = extent.width;
        fbInfo.height = extent.height;
        fbInfo.layers = 1;
        if (vkCreateFramebuffer(m_Context->GetDevice(), &fbInfo, nullptr, &m_HdrFramebuffer) != VK_SUCCESS)
            throw std::runtime_error("Failed to create HDR framebuffer");
    }

    void Renderer::DestroyHdrResources()
    {
        VkDevice device = m_Context->GetDevice();
        VmaAllocator allocator = m_Context->GetAllocator();
        if (m_HdrFramebuffer) vkDestroyFramebuffer(device, m_HdrFramebuffer, nullptr);
        if (m_HdrRenderPass) vkDestroyRenderPass(device, m_HdrRenderPass, nullptr);
        if (m_HdrImageView) vkDestroyImageView(device, m_HdrImageView, nullptr);
        if (m_HdrImage) vmaDestroyImage(allocator, m_HdrImage, m_HdrAllocation);
        m_HdrFramebuffer = VK_NULL_HANDLE;
        m_HdrRenderPass = VK_NULL_HANDLE;
        m_HdrImageView = VK_NULL_HANDLE;
        m_HdrImage = VK_NULL_HANDLE;
        m_HdrAllocation = VK_NULL_HANDLE;
    }

    void Renderer::CreatePresentPipeline()
    {
        auto vertCode = Pipeline::ReadFile("../../../Assets/Shaders/present_vert.spv");
        auto fragCode = Pipeline::ReadFile("../../../Assets/Shaders/present_frag.spv");
        VkShaderModule vertModule = Pipeline::CreateShaderModule(m_Context->GetDevice(), vertCode);
        VkShaderModule fragModule = Pipeline::CreateShaderModule(m_Context->GetDevice(), fragCode);

        VkPipelineShaderStageCreateInfo vertStage = {};
        vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStage.module = vertModule;
        vertStage.pName = "main";

        VkPipelineShaderStageCreateInfo fragStage = {};
        fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.module = fragModule;
        fragStage.pName = "main";

        VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

        VkPipelineVertexInputStateCreateInfo vertexInput = {};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = 0;
        vertexInput.vertexAttributeDescriptionCount = 0;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample = {};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil = {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState blendAttachment = {};
        blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
            | VK_COLOR_COMPONENT_A_BIT;
        blendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlend = {};
        colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlend.attachmentCount = 1;
        colorBlend.pAttachments = &blendAttachment;

        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkDescriptorSetLayoutBinding binding = {};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;
        if (vkCreateDescriptorSetLayout(m_Context->GetDevice(), &layoutInfo, nullptr, &m_PresentDescLayout) !=
            VK_SUCCESS)
            throw std::runtime_error("Failed to create present descriptor set layout");

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_PresentDescLayout;
        if (vkCreatePipelineLayout(m_Context->GetDevice(), &pipelineLayoutInfo, nullptr, &m_PresentPipelineLayout) !=
            VK_SUCCESS)
            throw std::runtime_error("Failed to create present pipeline layout");

        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlend;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_PresentPipelineLayout;
        pipelineInfo.renderPass = m_Swapchain->GetRenderPass();
        pipelineInfo.subpass = 0;

        if (vkCreateGraphicsPipelines(m_Context->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                      &m_PresentPipeline) != VK_SUCCESS)
            throw std::runtime_error("Failed to create present pipeline");

        vkDestroyShaderModule(m_Context->GetDevice(), fragModule, nullptr);
        vkDestroyShaderModule(m_Context->GetDevice(), vertModule, nullptr);

        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        if (vkCreateSampler(m_Context->GetDevice(), &samplerInfo, nullptr, &m_HdrSampler) != VK_SUCCESS)
            throw std::runtime_error("Failed to create HDR sampler");

        VkDescriptorPoolSize poolSize = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        if (vkCreateDescriptorPool(m_Context->GetDevice(), &poolInfo, nullptr, &m_PresentDescPool) != VK_SUCCESS)
            throw std::runtime_error("Failed to create present descriptor pool");

        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_PresentDescPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_PresentDescLayout;
        if (vkAllocateDescriptorSets(m_Context->GetDevice(), &allocInfo, &m_PresentDescSet) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate present descriptor set");

        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_HdrImageView;
        imageInfo.sampler = m_HdrSampler;
        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_PresentDescSet;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(m_Context->GetDevice(), 1, &write, 0, nullptr);
    }

    void Renderer::DestroyPresentPipeline()
    {
        VkDevice device = m_Context->GetDevice();
        if (m_PresentPipeline) vkDestroyPipeline(device, m_PresentPipeline, nullptr);
        if (m_PresentPipelineLayout) vkDestroyPipelineLayout(device, m_PresentPipelineLayout, nullptr);
        if (m_PresentDescLayout) vkDestroyDescriptorSetLayout(device, m_PresentDescLayout, nullptr);
        if (m_PresentDescPool) vkDestroyDescriptorPool(device, m_PresentDescPool, nullptr);
        if (m_HdrSampler) vkDestroySampler(device, m_HdrSampler, nullptr);
        m_PresentPipeline = VK_NULL_HANDLE;
        m_PresentPipelineLayout = VK_NULL_HANDLE;
        m_PresentDescLayout = VK_NULL_HANDLE;
        m_PresentDescPool = VK_NULL_HANDLE;
        m_HdrSampler = VK_NULL_HANDLE;
    }

    void Renderer::CreateCommandBuffers()
    {
        m_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        VkCommandBufferAllocateInfo alloc{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, m_Context->GetCommandPool(),
            VK_COMMAND_BUFFER_LEVEL_PRIMARY, (uint32_t)MAX_FRAMES_IN_FLIGHT
        };
        if (vkAllocateCommandBuffers(m_Context->GetDevice(), &alloc, m_CommandBuffers.data()) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate command buffers");
    }

    void Renderer::CreateSyncObjects()
    {
        m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        VkSemaphoreCreateInfo sInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            if (vkCreateSemaphore(m_Context->GetDevice(), &sInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS
                ||
                vkCreateSemaphore(m_Context->GetDevice(), &sInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS
                ||
                vkCreateFence(m_Context->GetDevice(), &fInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create sync objects");
        }
    }
    
        void Renderer::CreateGridPipeline() {
        auto vertCode = Pipeline::ReadFile("../../../Assets/Shaders/grid_vert.spv");
        auto fragCode = Pipeline::ReadFile("../../../Assets/Shaders/grid_frag.spv");

        VkShaderModule vertShaderModule = Pipeline::CreateShaderModule(m_Context->GetDevice(), vertCode);
        VkShaderModule fragShaderModule = Pipeline::CreateShaderModule(m_Context->GetDevice(), fragCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        // No vertex buffers (Vertices generated procedurally in the shader via gl_VertexIndex)
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE; // Important for full-screen quad rendering
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;   // Respect scene geometry depth
        depthStencil.depthWriteEnable = VK_FALSE; // Grid is translucent, don't occlude other transparents
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

        // Custom Blend Attachments configured to map safely over the GBuffer Layout
        std::array<VkPipelineColorBlendAttachmentState, 3> blendAttachments{};
        
        // 0: Albedo -> Apply Alpha Blending
        blendAttachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blendAttachments[0].blendEnable = VK_TRUE;
        blendAttachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachments[0].colorBlendOp = VK_BLEND_OP_ADD;
        blendAttachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blendAttachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
        
        // 1: Normal -> Disable writing to not screw up G-Buffer deferred lighting
        blendAttachments[1].colorWriteMask = 0;
        blendAttachments[1].blendEnable = VK_FALSE;
        
        // 2: WorldPos -> Disable writing
        blendAttachments[2].colorWriteMask = 0;
        blendAttachments[2].blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = static_cast<uint32_t>(blendAttachments.size());
        colorBlending.pAttachments = blendAttachments.data();

        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // Pass 2 View and Projection Matrices to the Shader directly 
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(glm::mat4) * 2;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(m_Context->GetDevice(), &pipelineLayoutInfo, nullptr, &m_GridPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create grid pipeline layout");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_GridPipelineLayout;
        
        // Using GBuffer RenderPass seamlessly!
        pipelineInfo.renderPass = m_GBuffer.GetRenderPass(); 
        pipelineInfo.subpass = 0;

        if (vkCreateGraphicsPipelines(m_Context->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_GridPipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create grid graphics pipeline");
        }

        vkDestroyShaderModule(m_Context->GetDevice(), fragShaderModule, nullptr);
        vkDestroyShaderModule(m_Context->GetDevice(), vertShaderModule, nullptr);
    }
}
