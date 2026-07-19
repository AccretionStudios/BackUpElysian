#include "DebugUI.h"
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>
#include <ImGuizmo/ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>
#include <string>
#include <cmath>
#include "ECS/Components.h"

namespace Elysian
{
    // Helper function to bound angles and prevent drift/jitter over time
    static float wrapDeg(float v)
    {
        v = std::fmod(v, 360.0f);
        if (v > 180.0f) v -= 360.0f;
        if (v < -180.0f) v += 360.0f;
        return v;
    }

    void DebugUI::Init(VulkanContext* context, Window* window, Swapchain* swapchain)
    {
        VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000}};
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000;
        pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        if (vkCreateDescriptorPool(context->GetDevice(), &pool_info, nullptr, &m_ImGuiDescriptorPool) != VK_SUCCESS)
            throw std::runtime_error("Failed to create ImGui descriptor pool");

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForVulkan(window->GetNativeWindow(), true);

        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.ApiVersion = VK_API_VERSION_1_0;
        init_info.Instance = context->GetInstance();
        init_info.PhysicalDevice = context->GetPhysicalDevice();
        init_info.Device = context->GetDevice();
        init_info.QueueFamily = context->FindQueueFamilies(context->GetPhysicalDevice()).graphicsFamily.value();
        init_info.Queue = context->GetGraphicsQueue();
        init_info.DescriptorPool = m_ImGuiDescriptorPool;
        init_info.MinImageCount = 2;
        init_info.ImageCount = static_cast<uint32_t>(swapchain->GetImageCount());
        init_info.PipelineInfoMain.RenderPass = swapchain->GetRenderPass();
        init_info.PipelineInfoMain.Subpass = 0;
        init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        ImGui_ImplVulkan_Init(&init_info);

        // Initialize ImGuizmo
        ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
    }

    void DebugUI::Cleanup(VulkanContext* context)
    {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        vkDestroyDescriptorPool(context->GetDevice(), m_ImGuiDescriptorPool, nullptr);
    }

    void DebugUI::BeginFrame()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
    }

    void DebugUI::DrawWindows(Scene* scene, Swapchain* swapchain, float deltaTime)
    {
        // Entities window
        ImGui::Begin("Entities");

        ImGui::Text("Entity List");
        ImGui::BeginChild("EntityListChild", ImVec2(0, 120), true);
        auto entityView = scene->m_ECSManager.GetRegistry().view<TransformComponent>();
        for (auto entity : entityView)
        {
            auto entityId = static_cast<uint32_t>(entt::to_integral(entity));
            bool isSelected = (entity == m_SelectedEntity);
            if (ImGui::Selectable(("Entity " + std::to_string(entityId)).c_str(), isSelected))
            {
                m_SelectedEntity = entity;
            }
        }
        ImGui::EndChild();

        if (m_SelectedEntity != entt::null && scene->m_ECSManager.HasComponent<TransformComponent>(m_SelectedEntity))
        {
            ImGui::Separator();
            ImGui::Text("Gizmo Controls");
            const char* ops[] = {"Translate", "Rotate", "Scale"};
            ImGui::Combo("Operation", &m_GizmoOperation, ops, 3);
            const char* modes[] = {"Local", "World"};
            ImGui::Combo("Mode", &m_GizmoMode, modes, 2);

            static bool useSnap = false;
            static float snapTrans = 1.0f;
            static float snapRot = 15.0f;
            static float snapScale = 0.1f;

            ImGui::Checkbox("Snap Enable", &useSnap);
            if (useSnap)
            {
                if (m_GizmoOperation == 0) ImGui::DragFloat("Snap Translation", &snapTrans, 0.1f);
                else if (m_GizmoOperation == 1) ImGui::DragFloat("Snap Rotation", &snapRot, 1.0f);
                else if (m_GizmoOperation == 2) ImGui::DragFloat("Snap Scale", &snapScale, 0.05f);
            }

            auto& transform = scene->m_ECSManager.GetComponent<TransformComponent>(m_SelectedEntity);
            ImGuizmo::OPERATION activeOp = (m_GizmoOperation == 0)
                                               ? ImGuizmo::TRANSLATE
                                               : (m_GizmoOperation == 1)
                                               ? ImGuizmo::ROTATE
                                               : ImGuizmo::SCALE;
            ImGuizmo::MODE activeMode = (m_GizmoMode == 0) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
            if (activeOp == ImGuizmo::ROTATE) activeMode = ImGuizmo::LOCAL;

            glm::mat4 viewMat = scene->m_Camera.GetViewMatrix();
            float aspect = (float)swapchain->GetExtent().width / (float)swapchain->GetExtent().height;
            glm::mat4 projMatOGL = glm::perspective(glm::radians(scene->m_Camera.FieldOfView), aspect, 0.1f, 100.0f);

            ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
            ImGuizmo::SetRect(0, 0, (float)m_ViewportWidth, (float)m_ViewportHeight);

            glm::mat4 model = transform.GetModelMatrix();
            float* modelPtr = glm::value_ptr(model);
            float deltaMatrix[16] = {0};
            float snapValues[3] = {0, 0, 0};
            if (useSnap)
            {
                if (activeOp == ImGuizmo::TRANSLATE) snapValues[0] = snapValues[1] = snapValues[2] = snapTrans;
                else if (activeOp == ImGuizmo::ROTATE) snapValues[0] = snapValues[1] = snapValues[2] = snapRot;
                else if (activeOp == ImGuizmo::SCALE) snapValues[0] = snapValues[1] = snapValues[2] = snapScale;
            }

            ImGuizmo::Manipulate(glm::value_ptr(viewMat), glm::value_ptr(projMatOGL),
                                 activeOp, activeMode,
                                 modelPtr, deltaMatrix,
                                 useSnap ? snapValues : nullptr);

            if (ImGuizmo::IsUsing())
            {
                if (activeOp == ImGuizmo::ROTATE)
                {
                    float dTr[3], dRt[3], dSc[3];
                    ImGuizmo::DecomposeMatrixToComponents(deltaMatrix, dTr, dRt, dSc);
                    transform.Rotation.x = wrapDeg(transform.Rotation.x + dRt[0]);
                    transform.Rotation.y = wrapDeg(transform.Rotation.y + dRt[1]);
                    transform.Rotation.z = wrapDeg(transform.Rotation.z + dRt[2]);
                }
                else
                {
                    glm::vec3 translation, rotation, scale;
                    ImGuizmo::DecomposeMatrixToComponents(modelPtr,
                                                          glm::value_ptr(translation),
                                                          glm::value_ptr(rotation),
                                                          glm::value_ptr(scale));
                    transform.Position = translation;
                    transform.Scale = scale;
                }
            }

            ImGui::Separator();
            ImGui::Text("Transform Editor");
            ImGui::DragFloat3("Position", glm::value_ptr(transform.Position), 0.05f);
            ImGui::DragFloat3("Rotation", glm::value_ptr(transform.Rotation), 1.0f);
            ImGui::DragFloat3("Scale", glm::value_ptr(transform.Scale), 0.05f);
            if (ImGui::Button("Reset Transform"))
            {
                transform.Position = glm::vec3(0.0f);
                transform.Rotation = glm::vec3(0.0f);
                transform.Scale = glm::vec3(1.0f);
            }
        }

        ImGui::End();

        // Lights window
        ImGui::Begin("Lights");
        ImGui::Text("Light List");
        ImGui::BeginChild("LightListChild", ImVec2(0, 120), true);
        auto lightView = scene->m_ECSManager.GetRegistry().view<LightComponent>();
        for (auto entity : lightView)
        {
            auto& light = lightView.get<LightComponent>(entity);
            const char* typeName = (light.type == LightType::Directional)
                                       ? "Dir"
                                       : (light.type == LightType::Point)
                                       ? "Point"
                                       : "Spot";
            ImGui::PushID(entt::to_integral(entity));
            if (ImGui::Selectable(
                ("Light " + std::to_string(entt::to_integral(entity)) + " (" + typeName + ")").c_str(),
                entity == m_SelectedEntity))
            {
                m_SelectedEntity = entity;
            }
            ImGui::PopID();
        }
        ImGui::EndChild();

        if (m_SelectedEntity != entt::null && scene->m_ECSManager.HasComponent<LightComponent>(m_SelectedEntity))
        {
            auto& light = scene->m_ECSManager.GetComponent<LightComponent>(m_SelectedEntity);
            ImGui::Separator();
            ImGui::Text("Light Properties");
            const char* lightTypes[] = {"Directional", "Point", "Spot"};
            int currentType = static_cast<int>(light.type);
            if (ImGui::Combo("Type", &currentType, lightTypes, 3))
                light.type = static_cast<LightType>(currentType);
            ImGui::ColorEdit3("Color", glm::value_ptr(light.color));
            ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.0f, 10.0f);
            if (light.type != LightType::Directional)
            {
                ImGui::DragFloat("Radius", &light.radius, 0.1f, 0.1f, 20.0f);
            }
            if (light.type == LightType::Spot)
            {
                float innerDeg = glm::degrees(light.innerAngle);
                float outerDeg = glm::degrees(light.outerAngle);
                if (ImGui::DragFloat("Inner Angle (deg)", &innerDeg, 1.0f, 0.0f, 90.0f))
                    light.innerAngle = glm::radians(innerDeg);
                if (ImGui::DragFloat("Outer Angle (deg)", &outerDeg, 1.0f, 0.0f, 90.0f))
                    light.outerAngle = glm::radians(outerDeg);
                if (light.innerAngle > light.outerAngle)
                    light.innerAngle = light.outerAngle;
            }
            // Ambient Strength for directional light
            if (light.type == LightType::Directional)
            {
                ImGui::DragFloat("Ambient Strength", &scene->m_AmbientStrength, 0.005f, 0.0f, 1.0f);
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Add Directional Light"))
        {
            auto newLight = scene->m_ECSManager.CreateEntity();
            scene->m_ECSManager.AddComponent<TransformComponent>(newLight);
            auto& lc = scene->m_ECSManager.AddComponent<LightComponent>(newLight);
            lc.type = LightType::Directional;
        }
        if (ImGui::Button("Add Point Light"))
        {
            auto newLight = scene->m_ECSManager.CreateEntity();
            scene->m_ECSManager.AddComponent<TransformComponent>(newLight);
            auto& lc = scene->m_ECSManager.AddComponent<LightComponent>(newLight);
            lc.type = LightType::Point;
            lc.radius = 3.0f;
        }
        if (ImGui::Button("Add Spot Light"))
        {
            auto newLight = scene->m_ECSManager.CreateEntity();
            scene->m_ECSManager.AddComponent<TransformComponent>(newLight);
            auto& lc = scene->m_ECSManager.AddComponent<LightComponent>(newLight);
            lc.type = LightType::Spot;
            lc.radius = 5.0f;
            lc.innerAngle = glm::radians(20.0f);
            lc.outerAngle = glm::radians(35.0f);
        }
        ImGui::End();

        // Camera Window
        ImGui::Begin("Camera");
        ImGui::SliderFloat("Field of View", &scene->m_Camera.FieldOfView, 30.0f, 120.0f);
        ImGui::Separator();
        ImGui::DragFloat("Movement Speed", &scene->m_Camera.MovementSpeed, 0.1f, 0.1f, 20.0f);
        ImGui::DragFloat("Mouse Sensitivity", &scene->m_Camera.MouseSensitivity, 0.005f, 0.01f, 1.0f);
        ImGui::DragFloat("Gamepad Sensitivity", &scene->m_Camera.GamepadSensitivity, 0.5f, 25.0f, 200.0f);
        ImGui::Separator();
        if (ImGui::Button("Reset Properties"))
        {
            scene->m_Camera.MovementSpeed = 2.5f;
            scene->m_Camera.MouseSensitivity = 0.1f;
            scene->m_Camera.GamepadSensitivity = 100.0f;
            scene->m_Camera.FieldOfView = 45.0f;
        }
        if (ImGui::Button("Reset Position"))
        {
            scene->m_Camera.Position = glm::vec3(0.0f, 0.0f, 5.0f);
            scene->m_Camera.Yaw = -90.0f;
            scene->m_Camera.Pitch = 0.0f;
        }
        ImGui::End();

        // Performance Profiler
        ImGui::Begin("Performance Profiler");
        float currentFps = ImGui::GetIO().Framerate;
        m_AvgFps = m_AvgFps + (currentFps - m_AvgFps) * 0.05f;
        float frameTimeMs = 1000.0f / (m_AvgFps > 0 ? m_AvgFps : 1.0f);
        ImGui::Text("Current FPS: %.1f", currentFps);
        ImGui::Text("Average FPS: %.1f", m_AvgFps);
        ImGui::Separator();
        ImGui::Text("Frame Time:  %.3f ms", frameTimeMs);
        m_FrameTimeHistory[m_FrameTimeOffset] = frameTimeMs;
        m_FrameTimeOffset = (m_FrameTimeOffset + 1) % 100;
        m_MaxFrameTime = 0.0f;
        for (int n = 0; n < 100; n++) if (m_FrameTimeHistory[n] > m_MaxFrameTime) m_MaxFrameTime = m_FrameTimeHistory[
            n];
        ImGui::PlotLines("##FrameTimeGraph", m_FrameTimeHistory, 100, m_FrameTimeOffset, "Frame Time (ms)", 0.0f,
                         m_MaxFrameTime * 1.2f, ImVec2(0, 80));
        ImGui::End();

        // Debug overlays for lights
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        ImVec2 screenSize = ImGui::GetIO().DisplaySize;
        glm::mat4 view = scene->m_Camera.GetViewMatrix();
        glm::mat4 proj = scene->m_Camera.GetProjectionMatrix(
            (float)swapchain->GetExtent().width / (float)swapchain->GetExtent().height);

        auto Project = [&](const glm::vec3& worldPos, ImVec2& outScreenPos) -> bool
        {
            glm::vec4 clipPos = proj * view * glm::vec4(worldPos, 1.0f);
            if (clipPos.w <= 0.0f) return false;
            glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
            outScreenPos = ImVec2((ndc.x * 0.5f + 0.5f) * screenSize.x, (ndc.y * 0.5f + 0.5f) * screenSize.y);
            return true;
        };

        // Point lights
        for (auto entity : lightView)
        {
            auto& light = lightView.get<LightComponent>(entity);
            if (light.type != LightType::Point) continue;
            if (!scene->m_ECSManager.HasComponent<TransformComponent>(entity)) continue;
            auto& transform = scene->m_ECSManager.GetComponent<TransformComponent>(entity);

            ImVec2 screenPos;
            if (!Project(transform.Position, screenPos)) continue;

            ImU32 whiteColor = IM_COL32(150, 150, 150, 255);

            std::string label = "Pointlight " + std::to_string(entt::to_integral(entity));
            ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
            ImVec2 textPos = ImVec2(screenPos.x - textSize.x * 0.5f, screenPos.y - textSize.y * 0.5f);
            drawList->AddText(textPos, whiteColor, label.c_str());

            // 3D wireframe rings
            const int segments = 32;
            const float radius = light.radius;
            const float PI = 3.14159265359f;

            auto drawCircle = [&](const std::function<glm::vec3(float)>& pointFunc, ImU32 col, float thickness = 1.5f)
            {
                ImVec2 prevScreen;
                bool prevValid = false;
                for (int i = 0; i <= segments; ++i)
                {
                    float theta = (float)i / (float)segments * 2.0f * PI;
                    glm::vec3 worldPoint = pointFunc(theta);
                    ImVec2 screenPoint;
                    bool valid = Project(worldPoint, screenPoint);
                    if (i > 0 && valid && prevValid)
                    {
                        drawList->AddLine(prevScreen, screenPoint, col, thickness);
                    }
                    prevScreen = screenPoint;
                    prevValid = valid;
                }
            };

            auto ringXY = [&](float theta) -> glm::vec3
            {
                return transform.Position + radius * glm::vec3(cos(theta), sin(theta), 0.0f);
            };
            drawCircle(ringXY, whiteColor);

            auto ringXZ = [&](float theta) -> glm::vec3
            {
                return transform.Position + radius * glm::vec3(cos(theta), 0.0f, sin(theta));
            };
            drawCircle(ringXZ, whiteColor);

            auto ringYZ = [&](float theta) -> glm::vec3
            {
                return transform.Position + radius * glm::vec3(0.0f, cos(theta), sin(theta));
            };
            drawCircle(ringYZ, whiteColor);
        }


        // Spotlights
        for (auto entity : lightView)
        {
            auto& light = lightView.get<LightComponent>(entity);
            if (light.type != LightType::Spot) continue;
            if (!scene->m_ECSManager.HasComponent<TransformComponent>(entity)) continue;
            auto& transform = scene->m_ECSManager.GetComponent<TransformComponent>(entity);

            ImVec2 screenOrigin;
            if (!Project(transform.Position, screenOrigin)) continue;
            ImU32 whiteColor = IM_COL32(150, 150, 150, 255);
            drawList->AddText(ImVec2(screenOrigin.x + 10, screenOrigin.y - 5), whiteColor, ("Spotlight " + std::to_string(entt::to_integral(entity))).c_str());

            // Direction from rotation
            glm::mat4 rotMat = glm::rotate(glm::mat4(1.0f), glm::radians(transform.Rotation.y), glm::vec3(0, 1, 0));
            rotMat = glm::rotate(rotMat, glm::radians(transform.Rotation.x), glm::vec3(1, 0, 0));
            rotMat = glm::rotate(rotMat, glm::radians(transform.Rotation.z), glm::vec3(0, 0, 1));
            glm::vec3 direction = rotMat * glm::vec4(0, 0, -1, 0);
            glm::vec3 baseCenter = transform.Position + direction * light.radius;
            ImVec2 screenBaseCenter;
            if (Project(baseCenter, screenBaseCenter))
                drawList->AddLine(screenOrigin, screenBaseCenter, whiteColor, 1.0f);

            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
            if (std::abs(glm::dot(up, direction)) > 0.999f) up = glm::vec3(0.0f, 0.0f, 1.0f);
            glm::vec3 right = glm::normalize(glm::cross(up, direction));
            glm::vec3 forward = glm::normalize(glm::cross(direction, right));

            float outerR = light.radius * std::tan(light.outerAngle);
            float innerR = light.radius * std::tan(light.innerAngle);
            const int segments = 32;
            ImVec2 prevOuter, prevInner;
            bool prevOuterValid = false, prevInnerValid = false;

            for (int j = 0; j <= segments; ++j)
            {
                float angle = (float)j / (float)segments * 2.0f * 3.14159265359f;
                glm::vec3 offsetOuter = (right * std::cos(angle) + forward * std::sin(angle)) * outerR;
                glm::vec3 offsetInner = (right * std::cos(angle) + forward * std::sin(angle)) * innerR;
                ImVec2 currOuter, currInner;
                bool currOuterValid = Project(baseCenter + offsetOuter, currOuter);
                bool currInnerValid = Project(baseCenter + offsetInner, currInner);

                if (j > 0)
                {
                    if (currOuterValid && prevOuterValid) drawList->AddLine(prevOuter, currOuter, whiteColor, 1.5f);
                    if (currInnerValid && prevInnerValid) drawList->AddLine(prevInner, currInner, whiteColor, 1.5f);
                }
                if (j % (segments / 4) == 0)
                {
                    if (currOuterValid) drawList->AddLine(screenOrigin, currOuter, whiteColor, 1.0f);
                    if (currInnerValid) drawList->AddLine(screenOrigin, currInner, whiteColor, 1.0f);
                }
                prevOuter = currOuter;
                prevOuterValid = currOuterValid;
                prevInner = currInner;
                prevInnerValid = currInnerValid;
            }
        }

        ImGui::Render();
    }

    void DebugUI::Render(VkCommandBuffer cmdBuffer)
    {
        ImDrawData* drawData = ImGui::GetDrawData();
        if (drawData && drawData->CmdListsCount > 0)
        {
            ImGui_ImplVulkan_RenderDrawData(drawData, cmdBuffer);
        }
    }
}
