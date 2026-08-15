#include "Platform/OpenGL/OpenGLContext.h"

#include "Core/Assert.h"
#include "Core/Log.h"

#include <GLFW/glfw3.h>
#include <glad/gl.h>

namespace HachimiEngine
{
    OpenGLContext::OpenGLContext(GLFWwindow* windowHandle)
        : m_WindowHandle(windowHandle)
    {
    }

    void OpenGLContext::Init()
    {
        HE_CORE_ASSERT(m_WindowHandle != nullptr);

        glfwMakeContextCurrent(m_WindowHandle);

        const int version = gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress));
        HE_CORE_ASSERT(version != 0);

        HE_CORE_INFO("OpenGL Info:");
        HE_CORE_INFO("  Vendor:   {}", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
        HE_CORE_INFO("  Renderer: {}", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        HE_CORE_INFO("  Version:  {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    }

    void OpenGLContext::SwapBuffers()
    {
        glfwSwapBuffers(m_WindowHandle);
    }
}
