#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>

namespace Elysian
{
    class Window
    {
    public:
        void Init(int width, int height, const std::string& title);
        void Cleanup();

        bool ShouldClose() const;
        void PollEvents() const;
        void WaitEvents() const;

        void GetFramebufferSize(int& width, int& height) const;
        GLFWwindow* GetNativeWindow() const { return m_Window; }

    private:
        GLFWwindow* m_Window = nullptr;
        int m_Width;
        int m_Height;
        std::string m_Title;
    };
}
