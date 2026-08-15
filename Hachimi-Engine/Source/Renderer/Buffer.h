#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

#include <glm/glm.hpp>

#include <initializer_list>
#include <string>
#include <vector>

namespace HachimiEngine
{
    enum class ShaderDataType
    {
        None = 0,
        Float,
        Float2,
        Float3,
        Float4,
        Int,
        Int2,
        Int3,
        Int4,
        Bool
    };

    uint32_t ShaderDataTypeSize(ShaderDataType type);
    uint32_t ShaderDataTypeComponentCount(ShaderDataType type);
    uint32_t ShaderDataTypeToOpenGLBaseType(ShaderDataType type);

    struct BufferElement
    {
        std::string Name;
        ShaderDataType Type = ShaderDataType::None;
        uint32_t Size = 0;
        size_t Offset = 0;
        bool Normalized = false;

        BufferElement() = default;
        BufferElement(ShaderDataType type, std::string name, bool normalized = false)
            : Name(std::move(name)), Type(type), Size(ShaderDataTypeSize(type)), Offset(0), Normalized(normalized)
        {
        }
    };

    class BufferLayout
    {
    public:
        BufferLayout() = default;
        BufferLayout(std::initializer_list<BufferElement> elements)
            : m_Elements(elements)
        {
            CalculateOffsetsAndStride();
        }

        const std::vector<BufferElement>& GetElements() const { return m_Elements; }
        uint32_t GetStride() const { return m_Stride; }

        std::vector<BufferElement>::iterator begin() { return m_Elements.begin(); }
        std::vector<BufferElement>::iterator end() { return m_Elements.end(); }
        std::vector<BufferElement>::const_iterator begin() const { return m_Elements.begin(); }
        std::vector<BufferElement>::const_iterator end() const { return m_Elements.end(); }

    private:
        void CalculateOffsetsAndStride();

    private:
        std::vector<BufferElement> m_Elements;
        uint32_t m_Stride = 0;
    };

    class VertexBuffer
    {
    public:
        virtual ~VertexBuffer() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

        virtual void SetData(const void* data, uint32_t size) = 0;

        virtual const BufferLayout& GetLayout() const = 0;
        virtual void SetLayout(const BufferLayout& layout) = 0;

        static Ref<VertexBuffer> Create(uint32_t size);
        static Ref<VertexBuffer> Create(const float* vertices, uint32_t size);
    };

    class IndexBuffer
    {
    public:
        virtual ~IndexBuffer() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

        virtual uint32_t GetCount() const = 0;

        static Ref<IndexBuffer> Create(const uint32_t* indices, uint32_t count);
    };
}
