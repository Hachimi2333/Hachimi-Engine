#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

namespace HachimiEngine
{
    class AssetDatabase;
    class DebugDraw;
    class EnvironmentMap;
    class MaterialResolver;
    class MeshData;
    class MeshLibrary;
    class PostProcessPass;
    class ShadowMap;
    class Shader;
    class ShaderLibrary;
    class TextureCache;
    class UniformBuffer;

    // Owns every renderer-side GPU resource and the render state that outlives a frame.
    //
    // One instance exists per process, created by Application before any layer attaches.
    // Its lifetime must stay inside the OpenGL context's, because the objects below
    // release GL handles in their destructors. SceneRenderer instances are built on top
    // of it and hold only per-frame state.
    //
    // The members are held behind pointers so this header stays free of the whole renderer; the
    // destructor is therefore defined in the source file, where every member type is complete.
    class RendererContext
    {
    public:
        RendererContext();
        ~RendererContext();

        RendererContext(const RendererContext&) = delete;
        RendererContext& operator=(const RendererContext&) = delete;

        // Creates the backend, loads the engine shaders and builds the built-in meshes.
        // Requires a current OpenGL context.
        void Init();
        // Releases every GPU resource. Safe to call when Init never ran.
        void Shutdown();

        bool IsInitialized() const { return m_Initialized; }

        // Asset services the renderer reads. They are owned by the application and only
        // referenced here, because a pass needs them to turn an AssetHandle into a texture.
        // Either may be null - a renderer with no asset pipeline falls back to the inline
        // material values on the item - so every reader has to handle that.
        void SetAssetDatabase(AssetDatabase* database) { m_AssetDatabase = database; }
        void SetTextureCache(TextureCache* textures) { m_TextureCache = textures; }
        AssetDatabase* GetAssetDatabase() const { return m_AssetDatabase; }
        TextureCache* GetTextureCache() const { return m_TextureCache; }

        MeshLibrary& GetMeshes() { return *m_MeshLibrary; }
        ShaderLibrary& GetShaders() { return *m_Shaders; }
        MaterialResolver& GetMaterials() { return *m_Materials; }
        DebugDraw& GetDebugDraw() { return *m_DebugDraw; }
        PostProcessPass& GetPostProcessPass() { return *m_PostProcessPass; }
        EnvironmentMap& GetEnvironmentMap() { return *m_EnvironmentMap; }
        ShadowMap& GetShadowMap() { return *m_ShadowMap; }
        // Per-view constant block. Passes that draw with the scene shader fill and upload it.
        UniformBuffer& GetFrameUniforms() { return *m_FrameUniforms; }

        const Ref<Shader>& GetDefaultShader() const { return m_DefaultShader; }
        const Ref<Shader>& GetGridShader() const { return m_GridShader; }
        const Ref<Shader>& GetDirectionalShadowShader() const { return m_DirectionalShadowShader; }
        const Ref<Shader>& GetSkyboxShader() const { return m_SkyboxShader; }

        const Ref<MeshData>& GetGridMesh() const { return m_GridMesh; }
        const Ref<MeshData>& GetSkyboxMesh() const { return m_SkyboxMesh; }

    private:
        // Declared before the objects they must outlive: members are destroyed in
        // reverse declaration order.
        Scope<ShaderLibrary> m_Shaders;
        Scope<MeshLibrary> m_MeshLibrary;
        Scope<MaterialResolver> m_Materials;
        Scope<PostProcessPass> m_PostProcessPass;
        Scope<DebugDraw> m_DebugDraw;
        Ref<UniformBuffer> m_FrameUniforms;

        Ref<Shader> m_DefaultShader;
        Ref<Shader> m_GridShader;
        Ref<Shader> m_DirectionalShadowShader;
        Ref<Shader> m_SkyboxShader;

        Ref<MeshData> m_GridMesh;
        Ref<MeshData> m_SkyboxMesh;

        Ref<ShadowMap> m_ShadowMap;
        Ref<EnvironmentMap> m_EnvironmentMap;

        // Not owned: the application outlives the renderer context.
        AssetDatabase* m_AssetDatabase = nullptr;
        TextureCache* m_TextureCache = nullptr;

        bool m_Initialized = false;
    };
}
