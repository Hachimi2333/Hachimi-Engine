#pragma once

#include "Asset/AssetHandle.h"
#include "Core/Base.h"
#include "Math/Math.h"

#include <string>
#include <string_view>

namespace HachimiEngine
{
    // Surface description stored as an Assets/Materials/*.hmaterial document.
    //
    // It carries handles rather than loaded objects, so a material asset never owns a GPU
    // resource and can be read, written and compared with no OpenGL context - which is what
    // keeps the asset layer testable and lets the renderer resolve it lazily.
    struct MaterialAsset
    {
        static constexpr const char* FileExtension = ".hmaterial";
        // Engine shaders are the only programs a material may name: they live in
        // Resources/Shaders and are loaded through Shader::CreateEngineShader. A material that
        // names anything else falls back to this one rather than rendering nothing.
        static constexpr const char* DefaultShaderName = "Default.glsl";

        std::string Shader = DefaultShaderName;
        Math::Vec4 BaseColor { 0.8f, 0.8f, 0.82f, 1.0f };
        // Invalid when the material has no albedo texture.
        AssetHandle AlbedoTexture;
        float Roughness = 0.6f;
        float Metallic = 0.05f;

        static MaterialAsset Default() { return MaterialAsset(); }

        static std::string Serialize(const MaterialAsset& material);
        // Missing fields keep their default and unknown fields are ignored, so a hand-edited
        // or newer file still loads. Returns false only when the text is not a usable document.
        static bool Deserialize(std::string_view text, MaterialAsset& out);
    };
}
