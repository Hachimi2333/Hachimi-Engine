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
        // Only the texture state belongs to the material program. The scalar surface values are
        // pushed per draw from the RenderItem, so a draw never depends on the order in which
        // these two writers ran.
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
