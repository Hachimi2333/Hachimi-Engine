#pragma once

#include "Platform/OpenGL/GraphicsContext.h"

struct GLFWwindow;

namespace HachimiEngine
{
    class OpenGLContext final : public GraphicsContext
    {
    public:
        explicit OpenGLContext(GLFWwindow* windowHandle);

        void Init() override;
        void SwapBuffers() override;

    private:
        GLFWwindow* m_WindowHandle = nullptr;
    };
}
