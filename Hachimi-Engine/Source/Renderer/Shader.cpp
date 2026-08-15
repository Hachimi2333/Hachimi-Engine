#include "Renderer/Shader.h"

#include "Core/Assert.h"
#include "Platform/OpenGL/OpenGLShader.h"

namespace HachimiEngine
{
    Ref<Shader> Shader::Create(const std::string& name, const std::string& vertexSource, const std::string& fragmentSource)
    {
        return CreateRef<OpenGLShader>(name, vertexSource, fragmentSource);
    }

    Ref<Shader> Shader::Create(const std::string& filepath)
    {
        return CreateRef<OpenGLShader>(filepath);
    }

    void ShaderLibrary::Add(const Ref<Shader>& shader)
    {
        Add(shader->GetName(), shader);
    }

    void ShaderLibrary::Add(const std::string& name, const Ref<Shader>& shader)
    {
        HE_CORE_ASSERT(!Exists(name));
        m_Shaders[name] = shader;
    }

    Ref<Shader> ShaderLibrary::Load(const std::string& filepath)
    {
        const Ref<Shader> shader = Shader::Create(filepath);
        Add(shader);
        return shader;
    }

    Ref<Shader> ShaderLibrary::Load(const std::string& name, const std::string& filepath)
    {
        const Ref<Shader> shader = Shader::Create(filepath);
        Add(name, shader);
        return shader;
    }

    Ref<Shader> ShaderLibrary::Get(const std::string& name)
    {
        HE_CORE_ASSERT(Exists(name));
        return m_Shaders[name];
    }

    bool ShaderLibrary::Exists(const std::string& name) const
    {
        return m_Shaders.contains(name);
    }
}
