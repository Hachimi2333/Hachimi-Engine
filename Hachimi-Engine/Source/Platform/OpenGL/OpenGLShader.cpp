#include "Platform/OpenGL/OpenGLShader.h"

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Math/Math.h"

#include <glad/gl.h>

#include <array>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

namespace HachimiEngine
{
    namespace
    {
        uint32_t CompileShaderStage(uint32_t type, const std::string& source)
        {
            const uint32_t shader = glCreateShader(type);
            const char* sourceCString = source.c_str();
            glShaderSource(shader, 1, &sourceCString, nullptr);
            glCompileShader(shader);

            GLint isCompiled = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
            if (isCompiled == GL_FALSE)
            {
                GLint maxLength = 0;
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

                std::vector<char> infoLog(static_cast<size_t>(maxLength));
                glGetShaderInfoLog(shader, maxLength, &maxLength, infoLog.data());

                glDeleteShader(shader);
                HE_CORE_ERROR("Shader compilation failed: {}", infoLog.data());
                HE_CORE_ASSERT(false);
                return 0;
            }

            return shader;
        }
    }

    OpenGLShader::OpenGLShader(std::string name, const std::string& vertexSource, const std::string& fragmentSource)
        : m_Name(std::move(name))
    {
        Compile({ { GL_VERTEX_SHADER, vertexSource }, { GL_FRAGMENT_SHADER, fragmentSource } });
    }

    OpenGLShader::OpenGLShader(const std::string& filepath)
        : m_Name(filepath)
    {
        const std::string source = ReadFile(filepath);
        Compile(PreProcess(source));

        const size_t pathSeparator = m_Name.find_last_of("/\\");
        if (pathSeparator != std::string::npos)
        {
            m_Name = m_Name.substr(pathSeparator + 1);
        }
    }

    OpenGLShader::~OpenGLShader()
    {
        glDeleteProgram(m_RendererID);
    }

    void OpenGLShader::Bind() const
    {
        glUseProgram(m_RendererID);
    }

    void OpenGLShader::Unbind() const
    {
        glUseProgram(0);
    }

    void OpenGLShader::SetInt(const std::string& name, int value)
    {
        glUniform1i(GetUniformLocation(name), value);
    }

    void OpenGLShader::SetFloat(const std::string& name, float value)
    {
        glUniform1f(GetUniformLocation(name), value);
    }

    void OpenGLShader::SetFloat2(const std::string& name, const Math::Vec2& value)
    {
        glUniform2f(GetUniformLocation(name), value.x, value.y);
    }

    void OpenGLShader::SetFloat3(const std::string& name, const Math::Vec3& value)
    {
        glUniform3f(GetUniformLocation(name), value.x, value.y, value.z);
    }

    void OpenGLShader::SetFloat4(const std::string& name, const Math::Vec4& value)
    {
        glUniform4f(GetUniformLocation(name), value.x, value.y, value.z, value.w);
    }

    void OpenGLShader::SetMat4(const std::string& name, const Math::Mat4& value)
    {
        glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, Math::ValuePtr(value));
    }

    std::string OpenGLShader::ReadFile(const std::string& filepath)
    {
        std::ifstream file(filepath, std::ios::in | std::ios::binary);
        HE_CORE_ASSERT(file.is_open());

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::unordered_map<uint32_t, std::string> OpenGLShader::PreProcess(const std::string& source)
    {
        std::unordered_map<uint32_t, std::string> shaderSources;

        constexpr const char* typeToken = "#type";
        const size_t typeTokenLength = std::strlen(typeToken);
        size_t tokenPosition = source.find(typeToken, 0);

        while (tokenPosition != std::string::npos)
        {
            const size_t endOfLine = source.find_first_of("\r\n", tokenPosition);
            HE_CORE_ASSERT(endOfLine != std::string::npos);

            const size_t typeBegin = tokenPosition + typeTokenLength + 1;
            const std::string type = source.substr(typeBegin, endOfLine - typeBegin);

            const size_t sourceBegin = source.find_first_not_of("\r\n", endOfLine);
            HE_CORE_ASSERT(sourceBegin != std::string::npos);

            const size_t nextTokenPosition = source.find(typeToken, sourceBegin);
            const size_t sourceEnd = nextTokenPosition == std::string::npos ? source.size() : nextTokenPosition;

            uint32_t stage = 0;
            if (type == "vertex")
            {
                stage = GL_VERTEX_SHADER;
            }
            else if (type == "fragment")
            {
                stage = GL_FRAGMENT_SHADER;
            }
            else
            {
                HE_CORE_ERROR("Unknown shader stage type: {}", type);
                HE_CORE_ASSERT(false);
            }

            shaderSources[stage] = source.substr(sourceBegin, sourceEnd - sourceBegin);
            tokenPosition = nextTokenPosition;
        }

        return shaderSources;
    }

    void OpenGLShader::Compile(const std::unordered_map<uint32_t, std::string>& shaderSources)
    {
        const uint32_t program = glCreateProgram();

        std::array<uint32_t, 2> shaderIDs = {};
        size_t shaderIDIndex = 0;

        for (const auto& [stage, source] : shaderSources)
        {
            const uint32_t shader = CompileShaderStage(stage, source);
            glAttachShader(program, shader);
            shaderIDs[shaderIDIndex++] = shader;
        }

        glLinkProgram(program);

        GLint isLinked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
        if (isLinked == GL_FALSE)
        {
            GLint maxLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

            std::vector<char> infoLog(static_cast<size_t>(maxLength));
            glGetProgramInfoLog(program, maxLength, &maxLength, infoLog.data());

            glDeleteProgram(program);
            for (const uint32_t shader : shaderIDs)
            {
                glDeleteShader(shader);
            }

            HE_CORE_ERROR("Shader link failed: {}", infoLog.data());
            HE_CORE_ASSERT(false);
            return;
        }

        for (const uint32_t shader : shaderIDs)
        {
            glDetachShader(program, shader);
            glDeleteShader(shader);
        }

        m_RendererID = program;
    }

    int OpenGLShader::GetUniformLocation(const std::string& name) const
    {
        const auto cacheIt = m_UniformLocationCache.find(name);
        if (cacheIt != m_UniformLocationCache.end())
        {
            return cacheIt->second;
        }

        const int location = glGetUniformLocation(m_RendererID, name.c_str());
        m_UniformLocationCache[name] = location;
        return location;
    }
}
