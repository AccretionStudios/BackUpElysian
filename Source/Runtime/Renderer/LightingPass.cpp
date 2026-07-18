#include "LightingPass.h"
#include <fstream>
#include <stdexcept>
#include <iostream>
#include "Pipeline.h"

namespace Elysian
{
    void LightingPass::Init(VulkanContext* context, GBuffer* gbuffer, VkRenderPass swapchainRenderPass)
    {
        m_Context = context;
        CreatePipeline(swapchainRenderPass);
    }

    void LightingPass::Cleanup()
    {
        VkDevice device = m_Context->GetDevice();
        if (m_Pipeline) vkDestroyPipeline(device, m_Pipeline, nullptr);
        if (m_PipelineLayout) vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
        if (m_GBufferDescLayout) vkDestroyDescriptorSetLayout(device, m_GBufferDescLayout, nullptr);
        if (m_LightDescLayout) vkDestroyDescriptorSetLayout(device, m_LightDescLayout, nullptr);
    }

    void LightingPass::CreatePipeline(VkRenderPass swapchainRenderPass)
    {
        VkDevice device = m_Context->GetDevice();

        // Load shaders
        std::string vertPath = "../../../Assets/Shaders/lighting_vert.spv";
        std::string fragPath = "../../../Assets/Shaders/lighting_frag.spv";

        std::cout << "Loading lighting vertex shader: " << vertPath << std::endl;
        std::vector<char> vertCode = Pipeline::ReadFile(vertPath);
        std::cout << "Loading lighting fragment shader: " << fragPath << std::endl;
        std::vector<char> fragCode = Pipeline::ReadFile(fragPath);

        VkShaderModule vertModule = Pipeline::CreateShaderModule(device, vertCode);
        VkShaderModule fragModule = Pipeline::CreateShaderModule(device, fragCode);

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

        // No vertex input – fullscreen triangle
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

        // Descriptor set layout for G-buffer textures (set = 0
        VkDescriptorSetLayoutBinding gbufferBindings[3] = {};
        for (int i = 0; i < 3; ++i)
        {
            gbufferBindings[i].binding = i;
            gbufferBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            gbufferBindings[i].descriptorCount = 1;
            gbufferBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        VkDescriptorSetLayoutCreateInfo gbufferLayoutInfo = {};
        gbufferLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        gbufferLayoutInfo.bindingCount = 3;
        gbufferLayoutInfo.pBindings = gbufferBindings;
        if (vkCreateDescriptorSetLayout(device, &gbufferLayoutInfo, nullptr, &m_GBufferDescLayout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create G-buffer descriptor set layout");

        // Descriptor set layout for light UBO (set = 1)
        VkDescriptorSetLayoutBinding lightBinding = {};
        lightBinding.binding = 0;
        lightBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        lightBinding.descriptorCount = 1;
        lightBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo lightLayoutInfo = {};
        lightLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        lightLayoutInfo.bindingCount = 1;
        lightLayoutInfo.pBindings = &lightBinding;
        if (vkCreateDescriptorSetLayout(device, &lightLayoutInfo, nullptr, &m_LightDescLayout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create light descriptor set layout");

        // Pipeline layout
        std::array<VkDescriptorSetLayout, 2> layouts = {m_GBufferDescLayout, m_LightDescLayout};
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
        pipelineLayoutInfo.pSetLayouts = layouts.data();
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create lighting pipeline layout");

        // Graphics pipeline
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
        pipelineInfo.layout = m_PipelineLayout;
        pipelineInfo.renderPass = swapchainRenderPass;
        pipelineInfo.subpass = 0;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline) != VK_SUCCESS)
            throw std::runtime_error("Failed to create lighting pipeline");

        vkDestroyShaderModule(device, fragModule, nullptr);
        vkDestroyShaderModule(device, vertModule, nullptr);

        std::cout << "Lighting pipeline created successfully." << std::endl;
    }

    void LightingPass::Record(VkCommandBuffer cmdBuffer)
    {
        // Bind the pipeline. Descriptor sets must be bound externally (Engine already does).
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
        vkCmdDraw(cmdBuffer, 3, 1, 0, 0); // fullscreen triangle
    }
}
