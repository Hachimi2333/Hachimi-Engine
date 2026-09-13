#include "Renderer/MaterialResolver.h"

#include "Asset/AssetDatabase.h"
#include "Asset/MaterialAsset.h"
#include "Asset/TextureCache.h"
#include "Core/Log.h"
#include "Renderer/Material.h"
#include "Renderer/RendererContext.h"
#include "Renderer/Shader.h"

#include <string>

namespace HachimiEngine
{
    MaterialResolver::MaterialResolver(RendererContext& renderers)
        : m_Renderers(renderers)
    {
    }

    MaterialResolver::~MaterialResolver() = default;

    void MaterialResolver::Clear()
    {
        m_Cache.clear();
    }

    Ref<Material> MaterialResolver::Resolve(const AssetHandle& handle)
    {
        if (!handle.IsValid() || handle.Type != AssetType::Material)
        {
            return nullptr;
        }

        AssetDatabase* database = m_Renderers.GetAssetDatabase();
        if (database == nullptr || !database->Contains(handle))
        {
            return nullptr;
        }

        // Rebuilt whenever the material's file changed, so editing a .hmaterial while the editor
        // is open is visible on the next frame without restarting anything.
        const uint64_t revision = database->GetAssetRevision(handle);
        const auto cached = m_Cache.find(handle);
        if (cached != m_Cache.end() && cached->second.Revision == revision)
        {
            return cached->second.Material;
        }

        std::string text;
        if (!database->ReadAssetText(handle, text))
        {
            HE_CORE_ERROR("Material asset '{}' could not be read", database->GetDisplayName(handle));
            m_Cache.erase(handle);
            return nullptr;
        }

        MaterialAsset materialAsset;
        if (!MaterialAsset::Deserialize(text, materialAsset))
        {
            HE_CORE_ERROR("Material asset '{}' is not a usable document", database->GetDisplayName(handle));
            m_Cache.erase(handle);
            return nullptr;
        }

        // A material may only name an engine shader: those load through the runtime data root and
        // work inside a game package. Anything else falls back instead of drawing nothing.
        ShaderLibrary& shaders = m_Renderers.GetShaders();
        Ref<Shader> shader;
        if (shaders.Exists(materialAsset.Shader))
        {
            shader = shaders.Get(materialAsset.Shader);
        }
        else
        {
            shader = shaders.LoadEngineShader(materialAsset.Shader);
        }

        if (shader == nullptr)
        {
            HE_CORE_ERROR("Material '{}' names shader '{}', which does not exist; using '{}'",
                database->GetDisplayName(handle),
                materialAsset.Shader,
                MaterialAsset::DefaultShaderName);

            materialAsset.Shader = MaterialAsset::DefaultShaderName;
            shader = shaders.Exists(materialAsset.Shader)
                ? shaders.Get(materialAsset.Shader)
                : shaders.LoadEngineShader(materialAsset.Shader);
        }

        Ref<Material> material = Material::Create(shader);

        if (materialAsset.AlbedoTexture.IsValid())
        {
            if (TextureCache* textures = m_Renderers.GetTextureCache())
            {
                const AssetMeta* textureMeta = database->GetMeta(materialAsset.AlbedoTexture);
                const TextureImportSettings settings = textureMeta != nullptr
                    ? textureMeta->Texture
                    : TextureImportSettings();

                const Ref<Texture2D> texture = textures->GetOrLoad(materialAsset.AlbedoTexture, settings);
                if (texture == nullptr)
                {
                    HE_CORE_WARN("Material '{}' references a texture that could not be loaded",
                        database->GetDisplayName(handle));
                }
                material->SetAlbedoTexture(texture);
            }
        }

        material->SetAlbedoColor(materialAsset.BaseColor);
        material->SetRoughness(materialAsset.Roughness);
        material->SetMetallic(materialAsset.Metallic);

        Entry entry;
        entry.Material = material;
        entry.Revision = revision;
        m_Cache[handle] = std::move(entry);
        return material;
    }
}
