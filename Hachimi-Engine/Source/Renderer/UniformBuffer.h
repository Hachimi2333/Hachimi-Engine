#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

#include <cstdint>

namespace HachimiEngine
{
    // Uniform block owned by the GPU side.
    //
    // SetData writes the whole block at once; the buffer stays bound to its binding point
    // for the lifetime of the program that declares it, so nothing has to re-bind per draw.
    class UniformBuffer
    {
    public:
        virtual ~UniformBuffer() = default;

        virtual void SetData(const void* data, uint32_t size, uint32_t offset = 0) = 0;
        virtual void Bind() const = 0;

        virtual uint32_t GetBindingPoint() const = 0;

        static Ref<UniformBuffer> Create(uint32_t size, uint32_t bindingPoint);
    };
}
