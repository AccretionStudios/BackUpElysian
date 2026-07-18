#include "Scene.h"
#include "ECS/Components.h"
#include "core/ResourceManager.h"
#include <stdexcept>
#include <iostream>

namespace Elysian
{
    void Scene::Init()
    {
        auto& resMgr = ResourceManager::Get();
        Mesh* sofaMesh = resMgr.LoadMesh("../../../Assets/Models/sofa.fbx");
        Mesh* houseMesh = resMgr.LoadMesh("../../../Assets/Models/viking_house.obj");

        // Main mesh
        auto mainEntity = m_ECSManager.CreateEntity();
        auto& mainTransform = m_ECSManager.AddComponent<TransformComponent>(mainEntity);
        m_ECSManager.AddComponent<MeshComponent>(mainEntity, sofaMesh);
        mainTransform.Scale = glm::vec3(0.5f);
        
        auto houseEntity = m_ECSManager.CreateEntity();
        auto& houseTransform = m_ECSManager.AddComponent<TransformComponent>(houseEntity);
        m_ECSManager.AddComponent<MeshComponent>(houseEntity, houseMesh);
        houseTransform.Position = glm::vec3(2.0f, 0.0f, 0.0f);
        houseTransform.Scale = glm::vec3(0.5f);
        
        auto secondSofa = m_ECSManager.CreateEntity();
        auto& sofa2Transform = m_ECSManager.AddComponent<TransformComponent>(secondSofa);
        m_ECSManager.AddComponent<MeshComponent>(secondSofa, sofaMesh);
        sofa2Transform.Position = glm::vec3(-2.0f, 0.0f, 0.0f);
        sofa2Transform.Scale = glm::vec3(0.5f);

        // CREATE LIGHT ENTITIES
        // Directional light
        auto dirLight = m_ECSManager.CreateEntity();
        m_ECSManager.AddComponent<TransformComponent>(dirLight);
        m_ECSManager.AddComponent<LightComponent>(dirLight);
        auto& dirLightComp = m_ECSManager.GetComponent<LightComponent>(dirLight);
        dirLightComp.type = LightType::Directional;
        dirLightComp.color = glm::vec3(1.0f);
        dirLightComp.intensity = 1.0f;
        auto& dirLightTrans = m_ECSManager.GetComponent<TransformComponent>(dirLight);
        dirLightTrans.Rotation = glm::vec3(60.0f, -120.0f, 0.0f); // direction = forward vector from rotation

        // Point lights
        auto pointLight1 = m_ECSManager.CreateEntity();
        m_ECSManager.AddComponent<TransformComponent>(pointLight1);
        m_ECSManager.AddComponent<LightComponent>(pointLight1);
        auto& pl1Comp = m_ECSManager.GetComponent<LightComponent>(pointLight1);
        pl1Comp.type = LightType::Point;
        pl1Comp.color = glm::vec3(1.0f, 0.2f, 0.2f);
        pl1Comp.intensity = 1.0f;
        pl1Comp.radius = 1.0f;
        auto& pl1Trans = m_ECSManager.GetComponent<TransformComponent>(pointLight1);
        pl1Trans.Position = glm::vec3(-3.0f, 1.0f, 2.0f);

        auto pointLight2 = m_ECSManager.CreateEntity();
        m_ECSManager.AddComponent<TransformComponent>(pointLight2);
        m_ECSManager.AddComponent<LightComponent>(pointLight2);
        auto& pl2Comp = m_ECSManager.GetComponent<LightComponent>(pointLight2);
        pl2Comp.type = LightType::Point;
        pl2Comp.color = glm::vec3(0.2f, 0.5f, 1.0f);
        pl2Comp.intensity = 1.0f;
        pl2Comp.radius = 1.0f;
        auto& pl2Trans = m_ECSManager.GetComponent<TransformComponent>(pointLight2);
        pl2Trans.Position = glm::vec3(-2.0f, -1.0f, 2.0f);

        // Spot lights
        auto spotLight1 = m_ECSManager.CreateEntity();
        m_ECSManager.AddComponent<TransformComponent>(spotLight1);
        m_ECSManager.AddComponent<LightComponent>(spotLight1);
        auto& sl1Comp = m_ECSManager.GetComponent<LightComponent>(spotLight1);
        sl1Comp.type = LightType::Spot;
        sl1Comp.color = glm::vec3(1.0f, 0.8f, 0.2f);
        sl1Comp.intensity = 1.0f;
        sl1Comp.radius = 1.0f;
        sl1Comp.innerAngle = glm::radians(10.0f);
        sl1Comp.outerAngle = glm::radians(20.0f);
        auto& sl1Trans = m_ECSManager.GetComponent<TransformComponent>(spotLight1);
        sl1Trans.Position = glm::vec3(3.0f, 2.0f, 1.0f);
        sl1Trans.Rotation = glm::vec3(0.0f, -45.0f, 0.0f); // direction

        auto spotLight2 = m_ECSManager.CreateEntity();
        m_ECSManager.AddComponent<TransformComponent>(spotLight2);
        m_ECSManager.AddComponent<LightComponent>(spotLight2);
        auto& sl2Comp = m_ECSManager.GetComponent<LightComponent>(spotLight2);
        sl2Comp.type = LightType::Spot;
        sl2Comp.color = glm::vec3(0.2f, 1.0f, 0.3f);
        sl2Comp.intensity = 1.5f;
        sl2Comp.radius = 1.0f;
        sl2Comp.innerAngle = glm::radians(15.0f);
        sl2Comp.outerAngle = glm::radians(30.0f);
        auto& sl2Trans = m_ECSManager.GetComponent<TransformComponent>(spotLight2);
        sl2Trans.Position = glm::vec3(-1.0f, 2.5f, -2.0f);
        sl2Trans.Rotation = glm::vec3(0.0f, 30.0f, 0.0f);

        std::cout << "Initialized scene with mesh entities and light entities." << std::endl;
    }
}
