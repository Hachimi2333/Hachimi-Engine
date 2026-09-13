#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

#include <entt/entt.hpp>
#include <yaml-cpp/yaml.h>

#include <string>
#include <vector>

namespace HachimiEngine
{
    class AssetDatabase;
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
        // Current .hscene layout.
        // Version 1 was the pre-registry format, which stored enumerators as integers and keyed
        // hierarchy children by UUID; it is deliberately not readable. Version 2 stored components
        // through the registry but referenced materials and scripts by inline values and path
        // strings. Version 3 references assets by UUID.
        static constexpr int CurrentFormatVersion = 3;

        explicit SceneSerializer(const Ref<Scene>& scene);

        // Returns false when the file could not be written.
        bool Serialize(const std::string& filepath);
        bool Deserialize(const std::string& filepath);

        // One entity, written as a standalone YAML document.
        //
        // The undo system uses this to remember an entity and its whole subtree across a delete,
        // which is the only way to bring back a parent together with children that carry
        // components: a plain handle snapshot cannot survive the registry losing the entities.
        static std::string SerializeEntitiesToString(const Scene& scene, const std::vector<Entity>& entities);
        // Recreates the entities a previous call wrote. Returns how many were created; the
        // original UUIDs are preserved, so references to them stay valid across undo.
        static size_t DeserializeEntitiesFromString(Scene& scene, const std::string& text);
        // Writes just one component of one entity, for the undo system to remember before the
        // component is removed.
        static std::string SerializeComponentToString(const Scene& scene, Entity entity, entt::id_type typeId);
        // Puts the components a previous call wrote back onto the entity, which keeps its own
        // identity, hierarchy and every component the snapshot did not carry.
        static bool DeserializeComponentFromString(Scene& scene, Entity entity, const std::string& text);
        // Asset database component deserialization resolves legacy path references through.
        // Set by the application for the lifetime of a project, so the serializers do not have
        // to thread it through the component descriptor signatures.
        static void SetAssetDatabase(AssetDatabase* database) { s_AssetDatabase = database; }
        static AssetDatabase* GetAssetDatabase() { return s_AssetDatabase; }
    private:
        void SerializeEntity(YAML::Emitter& out, Entity entity);
        static void SerializeEntity(YAML::Emitter& out, const Scene& scene, Entity entity);
        void DeserializeEntity(const YAML::Node& entityNode, Scene& scene);

    private:
        Ref<Scene> m_Scene;

        static AssetDatabase* s_AssetDatabase;
    };
}
