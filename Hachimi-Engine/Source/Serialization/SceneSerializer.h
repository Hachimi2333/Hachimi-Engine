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
    //
    // Components are written and read through ComponentRegistry rather than one block of hand
    // written code per type, so a component that is registered is also saved and loaded. The
    // file carries a FormatVersion: an unrecognised version is refused instead of being parsed
    // as if it had this layout.
    class SceneSerializer
    {
    public:
        // Current .hscene layout. 1 was the pre-registry format, which stored enumerators as
        // integers and keyed hierarchy children by UUID; it is deliberately not readable.
        static constexpr int CurrentFormatVersion = 2;

        explicit SceneSerializer(const Ref<Scene>& scene);

        // Returns false when the file could not be written.
        bool Serialize(const std::string& filepath);
        bool Deserialize(const std::string& filepath);

    private:
        void SerializeEntity(YAML::Emitter& out, Entity entity);
        void DeserializeEntity(const YAML::Node& entityNode, Scene& scene);

    private:
        Ref<Scene> m_Scene;
    };
}
