#pragma once

#include "Renderer/Shader.h"

#include <glm/glm.hpp>

#include <string>
#include <unordered_map>

namespace HachimiEngine
{
    class OpenGLShader final : public Shader
    {
    public:
        OpenGLShader(std::string name, const std::string& vertexSource, const std::string& fragmentSource);
        OpenGLShader(const std::string& filepath);
        ~OpenGLShader() override;

        void Bind() const override;
        void Unbind() const override;

        void SetInt(const std::string& name, int value) override;
        void SetFloat(const std::string& name, float value) override;
        void SetFloat2(const std::string& name, const glm::vec2& value) override;
        void SetFloat3(const std::string& name, const glm::vec3& value) override;
        void SetFloat4(const std::string& name, const glm::vec4& value) override;
        void SetMat4(const std::string& name, const glm::mat4& value) override;

        const std::string& GetName() const override { return m_Name; }

    private:
        static std::string ReadFile(const std::string& filepath);
        static std::unordered_map<uint32_t, std::string> PreProcess(const std::string& source);

        void Compile(const std::unordered_map<uint32_t, std::string>& shaderSources);
        int GetUniformLocation(const std::string& name) const;

    private:
        uint32_t m_RendererID = 0;
        std::string m_Name;
        mutable std::unordered_map<std::string, int> m_UniformLocationCache;
    };
}
