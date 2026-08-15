#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/Shader.h"
#include "Renderer/Texture.h"

#include <glm/glm.hpp>

namespace HachimiEngine
{
    // Simple PBR-ish surface parameters consumed by the scene shader.
    class Material
    {
    public:
        Material() = default;
        Material(const Ref<Shader>& shader);

        void Bind() const;

        Ref<Shader> GetShader() const { return m_Shader; }
        void SetShader(const Ref<Shader>& shader) { m_Shader = shader; }

        Ref<Texture2D> GetAlbedoTexture() const { return m_AlbedoTexture; }
        void SetAlbedoTexture(const Ref<Texture2D>& texture) { m_AlbedoTexture = texture; }

        glm::vec4 GetAlbedoColor() const { return m_AlbedoColor; }
        void SetAlbedoColor(const glm::vec4& color) { m_AlbedoColor = color; }

        float GetRoughness() const { return m_Roughness; }
        void SetRoughness(float roughness) { m_Roughness = roughness; }

        float GetMetallic() const { return m_Metallic; }
        void SetMetallic(float metallic) { m_Metallic = metallic; }

        static Ref<Material> Create(const Ref<Shader>& shader = nullptr);

    private:
        Ref<Shader> m_Shader;
        Ref<Texture2D> m_AlbedoTexture;
        glm::vec4 m_AlbedoColor { 0.8f, 0.8f, 0.82f, 1.0f };
        float m_Roughness = 0.6f;
        float m_Metallic = 0.05f;
    };
}
