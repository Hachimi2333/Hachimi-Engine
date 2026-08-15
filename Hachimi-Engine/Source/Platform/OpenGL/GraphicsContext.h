#pragma once

#include "Core/Base.h"

namespace HachimiEngine
{
    // Abstraction over the windowing system's current OpenGL context.
    class GraphicsContext
    {
    public:
        virtual ~GraphicsContext() = default;

        virtual void Init() = 0;
        virtual void SwapBuffers() = 0;
    };
}
