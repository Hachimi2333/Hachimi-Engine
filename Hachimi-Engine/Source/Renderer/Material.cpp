#include "Renderer/Material.h"

namespace HachimiEngine
{
    Material::Material(const Ref<Shader>& shader)
        : m_Shader(shader)
    {
    }

    void Material::Bind() const
    {
        if (m_Shader == nullptr)
        {
            return;
        }

        m_Shader->Bind();
        m_Shader->SetFloat4("u_AlbedoColor", m_AlbedoColor);
        m_Shader->SetFloat("u_Roughness", m_Roughness);
        m_Shader->SetFloat("u_Metallic", m_Metallic);
        m_Shader->SetInt("u_HasAlbedoTexture", m_AlbedoTexture != nullptr ? 1 : 0);

        if (m_AlbedoTexture != nullptr)
        {
            m_AlbedoTexture->Bind(0);
        }
    }

    Ref<Material> Material::Create(const Ref<Shader>& shader)
    {
        return CreateRef<Material>(shader);
    }
}
