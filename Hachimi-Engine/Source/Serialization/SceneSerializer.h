#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

#include <yaml-cpp/yaml.h>

#include <string>

namespace HachimiEngine
{
    class Scene;
    class Entity;

    // YAML scene persistence for .hscene files.
    class SceneSerializer
    {
    public:
        explicit SceneSerializer(const Ref<Scene>& scene);

        void Serialize(const std::string& filepath);
        bool Deserialize(const std::string& filepath);

    private:
        void SerializeEntity(YAML::Emitter& out, Entity entity);
        void DeserializeEntity(YAML::Node entityNode, Scene& scene);

    private:
        Ref<Scene> m_Scene;
    };
}
