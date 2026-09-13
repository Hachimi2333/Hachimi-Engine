#include "Renderer/RendererContext.h"

#include "Core/Assert.h"
#include "Renderer/DebugDraw.h"
#include "Renderer/EnvironmentMap.h"
#include "Renderer/MaterialResolver.h"
#include "Renderer/MeshFactory.h"
#include "Renderer/MeshLibrary.h"
#include "Renderer/PostProcessPass.h"
#include "Renderer/Renderer.h"
#include "Renderer/FrameUniforms.h"
#include "Renderer/Shader.h"
#include "Renderer/ShadowMap.h"
#include "Renderer/UniformBuffer.h"

namespace HachimiEngine
{
    namespace
    {
        constexpr uint32_t ShadowMapResolution = 2048;

        const char* const DefaultShaderName = "Default.glsl";
        const char* const GridShaderName = "Grid.glsl";
        const char* const DirectionalShadowShaderName = "DirectionalShadow.glsl";
        const char* const SkyboxShaderName = "Skybox.glsl";

        // Matches `layout(std140, binding = 0)` on FrameBlock in Default.glsl.
        constexpr uint32_t FrameUniformBindingPoint = 0;
    }

    RendererContext::RendererContext() = default;

    RendererContext::~RendererContext()
    {
        Shutdown();
    }

    void RendererContext::Init()
    {
        if (m_Initialized)
        {
            HE_CORE_WARN("Renderer context is already initialized");
            return;
        }

        Renderer::Init();

        m_Shaders = CreateScope<ShaderLibrary>();
        m_MeshLibrary = CreateScope<MeshLibrary>();
        // The resolver reads the asset database, the shaders and the texture cache through this
        // context, so it is built last: everything it reaches for already exists.
        m_Materials = CreateScope<MaterialResolver>(*this);

        // Binding point 0 is the one the scene shader's FrameBlock declares, so no
        // glUniformBlockBinding call is needed anywhere.
        m_FrameUniforms = UniformBuffer::Create(sizeof(FrameUniforms), FrameUniformBindingPoint);

        m_DefaultShader = m_Shaders->LoadEngineShader(DefaultShaderName);
        m_GridShader = m_Shaders->LoadEngineShader(GridShaderName);
        m_DirectionalShadowShader = m_Shaders->LoadEngineShader(DirectionalShadowShaderName);
        m_SkyboxShader = m_Shaders->LoadEngineShader(SkyboxShaderName);

        m_GridMesh = MeshFactory::CreateGrid();
        m_SkyboxMesh = MeshFactory::CreateCube(2.0f);
        m_ShadowMap = ShadowMap::Create(ShadowMapResolution, ShadowMapResolution);        m_EnvironmentMap = CreateRef<EnvironmentMap>(128);

        m_PostProcessPass = CreateScope<PostProcessPass>();
        m_DebugDraw = CreateScope<DebugDraw>();

        m_Initialized = true;
    }

    void RendererContext::Shutdown()
    {
        if (!m_Initialized)
        {
            return;
        }

        // Declared last, released first: these own GL handles and must go before the
        // backend they were created through.
        m_DebugDraw.reset();
        m_PostProcessPass.reset();
        m_FrameUniforms.reset();

        m_EnvironmentMap.reset();
        m_ShadowMap.reset();

        // Releasing the mesh library frees the uploaded vertex arrays, and the material resolver
        // frees the materials it built on top of them.
        m_Materials.reset();
        m_MeshLibrary.reset();

        m_SkyboxMesh.reset();
        m_GridMesh.reset();

        m_DefaultShader.reset();
        m_GridShader.reset();
        m_DirectionalShadowShader.reset();
        m_SkyboxShader.reset();
        m_Shaders.reset();

        Renderer::Shutdown();
        m_Initialized = false;
    }
}
