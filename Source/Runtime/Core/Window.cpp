#include "Window.h"
#include <stdexcept>

namespace Elysian
{
    void Window::Init(int width, int height, const std::string& title)
    {
        m_Width = width;
        m_Height = height;
        m_Title = title;

        if (!glfwInit())
        {
            throw std::runtime_error("Failed to init GLFW");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        m_Window = glfwCreateWindow(m_Width, m_Height, m_Title.c_str(), nullptr, nullptr);
    }

    void Window::Cleanup()
    {
        glfwDestroyWindow(m_Window);
        glfwTerminate();
    }

    bool Window::ShouldClose() const
    {
        return glfwWindowShouldClose(m_Window);
    }

    void Window::PollEvents() const
    {
        glfwPollEvents();
    }

    void Window::WaitEvents() const
    {
        glfwWaitEvents();
    }

    void Window::GetFramebufferSize(int& width, int& height) const
    {
        glfwGetFramebufferSize(m_Window, &width, &height);
    }
}
