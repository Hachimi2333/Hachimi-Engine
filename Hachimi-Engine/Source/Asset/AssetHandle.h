#pragma once

#include "Core/Base.h"
#include "Core/UUID.h"

#include <cstddef>
#include <functional>

namespace HachimiEngine
{
    // Kinds of content the asset database indexes. Enumerators are stored by name in
    // .meta sidecar files, so the order may change without invalidating anything.
    enum class AssetType
    {
        None = 0,
        Texture = 1,
        Material = 2,
        Scene = 3,
        Script = 4,
        Project = 5
    };

    // Stable reference to one asset.
    //
    // The UUID is the identity and survives renaming or moving the file; the path is only
    // how the database finds the bytes, which is what makes "rename a texture, keep every
    // reference" work. AssetType travels with the handle rather than being looked up, so a
    // component field or an inspector widget knows what it is holding without a database
    // round trip, and so a texture handle can never be assigned to a material field.
    struct AssetHandle
    {
        UUID ID = UUID::Invalid();
        AssetType Type = AssetType::None;

        bool IsValid() const { return ID != UUID::Invalid() && Type != AssetType::None; }
        explicit operator bool() const { return IsValid(); }

        bool operator==(const AssetHandle& other) const { return ID == other.ID && Type == other.Type; }
        bool operator!=(const AssetHandle& other) const { return !(*this == other); }

        static AssetHandle Invalid() { return AssetHandle(); }
        static AssetHandle From(UUID id, AssetType type) { return AssetHandle { id, type }; }
    };
}

namespace std
{
    template<>
    struct hash<HE::AssetHandle>
    {
        size_t operator()(const HE::AssetHandle& handle) const noexcept
        {
            // Mix the type into the identity: two assets of different kinds can share an id in
            // a hand-edited .meta file, and they must not collide in a hash map.
            const size_t idHash = std::hash<uint64_t>()(handle.ID.GetValue());
            return idHash ^ (static_cast<size_t>(handle.Type) * 0x9E3779B97F4A7C15ull);
        }
    };
}
