#pragma once
#include <entt/entt.hpp>
#include "Components.h"

namespace Elysian
{
    class ECSManager
    {
    public:
        entt::registry& GetRegistry() { return m_Registry; }

        entt::entity CreateEntity()
        {
            return m_Registry.create();
        }

        void DestroyEntity(entt::entity entity)
        {
            m_Registry.destroy(entity);
        }

        template <typename T, typename... Args>
        T& AddComponent(entt::entity entity, Args&&... args)
        {
            return m_Registry.emplace<T>(entity, std::forward<Args>(args)...);
        }

        template <typename T>
        T& GetComponent(entt::entity entity)
        {
            return m_Registry.get<T>(entity);
        }

        template <typename T>
        bool HasComponent(entt::entity entity)
        {
            return m_Registry.all_of<T>(entity);
        }

    private:
        entt::registry m_Registry;
    };
}
