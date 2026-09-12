#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

namespace HachimiEngine
{
    class DebugDraw;
    class EnvironmentMap;
    class Material;
    class MeshData;
    class MeshLibrary;
    class PostProcessPass;
    class ShadowMap;
    class Shader;
    class ShaderLibrary;
    class UniformBuffer;

    // Owns every renderer-side GPU resource and the render state that outlives a frame.
    //
    // One instance exists per process, created by Application before any layer attaches.
    // Its lifetime must stay inside the OpenGL context's, because the objects below
    // release GL handles in their destructors. SceneRenderer instances are built on top
    // of it and hold only per-frame state.
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

        MeshLibrary& GetMeshes() { return *m_MeshLibrary; }
        ShaderLibrary& GetShaders() { return *m_Shaders; }
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

        bool m_Initialized = false;
    };
}
