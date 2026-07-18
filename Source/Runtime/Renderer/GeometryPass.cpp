#include "GeometryPass.h"
#include <fstream>
#include <stdexcept>
#include "Pipeline.h"
#include "VulkanTypes.h"
#include <glm/gtc/type_ptr.hpp>

namespace Elysian
{
    void GeometryPass::Init(VulkanContext* context, GBuffer* gbuffer, VkDescriptorSetLayout meshDescriptorSetLayout)
    {
        m_Context = context;
        m_GBuffer = gbuffer;
        CreatePipeline(meshDescriptorSetLayout);
    }

    void GeometryPass::Cleanup()
    {
        if (m_PipelineCCW) vkDestroyPipeline(m_Context->GetDevice(), m_PipelineCCW, nullptr);
        if (m_PipelineCW) vkDestroyPipeline(m_Context->GetDevice(), m_PipelineCW, nullptr);
        if (m_PipelineLayout) vkDestroyPipelineLayout(m_Context->GetDevice(), m_PipelineLayout, nullptr);
    }

    void GeometryPass::CreatePipeline(VkDescriptorSetLayout meshDescriptorSetLayout)
    {
        auto vertCode = Pipeline::ReadFile("../../../Assets/Shaders/geometry_vert.spv");
        auto fragCode = Pipeline::ReadFile("../../../Assets/Shaders/geometry_frag.spv");

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

        auto bindingDesc = Vertex::getBindingDescription();
        auto attrDescs = Vertex::getAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInput = {};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &bindingDesc;
        vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDescs.size());
        vertexInput.pVertexAttributeDescriptions = attrDescs.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo multisample = {};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil = {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendAttachmentState colorBlendAttachments[3] = {};
        for (int i = 0; i < 3; i++)
        {
            colorBlendAttachments[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachments[i].blendEnable = VK_FALSE;
        }

        VkPipelineColorBlendStateCreateInfo colorBlend = {};
        colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlend.attachmentCount = 3;
        colorBlend.pAttachments = colorBlendAttachments;

        VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = 2;
        dynamicState.pDynamicStates = dynamicStates;

        // Descriptor set layout (set=0) for UBO (view and proj)
        VkDescriptorSetLayout layouts[] = {meshDescriptorSetLayout};

        // Push constant range for model matrix
        VkPushConstantRange pushConstantRange = {};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(glm::mat4);

        VkPipelineLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = layouts;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(m_Context->GetDevice(), &layoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create geometry pipeline layout");

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
        pipelineInfo.renderPass = m_GBuffer->GetRenderPass();
        pipelineInfo.subpass = 0;

        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        if (vkCreateGraphicsPipelines(m_Context->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_PipelineCCW)
            != VK_SUCCESS)
            throw std::runtime_error("Failed to create CCW geometry pipeline");

        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        if (vkCreateGraphicsPipelines(m_Context->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_PipelineCW)
            != VK_SUCCESS)
            throw std::runtime_error("Failed to create CW geometry pipeline");

        vkDestroyShaderModule(m_Context->GetDevice(), fragModule, nullptr);
        vkDestroyShaderModule(m_Context->GetDevice(), vertModule, nullptr);
    }

    void GeometryPass::Record(VkCommandBuffer cmdBuffer, uint32_t currentFrame, VkDescriptorSet meshDescriptorSet,
                              VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t indexCount, const glm::mat4& model,
                              VkFrontFace frontFace)
    {
        VkPipeline activePipeline = (frontFace == VK_FRONT_FACE_CLOCKWISE) ? m_PipelineCW : m_PipelineCCW;
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipeline);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &meshDescriptorSet,
                                0, nullptr);

        // Push model matrix
        vkCmdPushConstants(cmdBuffer, m_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4),
                           glm::value_ptr(model));

        VkBuffer vertexBuffers[] = {vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmdBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmdBuffer, indexCount, 1, 0, 0, 0);
    }
}
