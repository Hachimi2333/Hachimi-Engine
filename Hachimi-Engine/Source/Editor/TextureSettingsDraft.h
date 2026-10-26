#pragma once

#include "Asset/AssetMeta.h"

namespace HachimiEngine
{
    // Binding the same asset on a later frame must retain unapplied edits.
    struct TextureSettingsDraft
    {
        AssetHandle For;
        TextureImportSettings Value;
        bool Changed = false;

        void Bind(AssetHandle handle, const AssetMeta& meta)
        {
            if (For == handle) return;
            Revert(handle, meta);
        }
        void Revert(AssetHandle handle, const AssetMeta& meta)
        {
            For = handle;
            Value = meta.Texture;
            Changed = false;
        }
    };
}
