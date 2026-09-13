#pragma once

#include "Core/Memory.h"
#include "Scripting/ScriptRuntime.h"

#include <filesystem>
#include <string>

namespace HachimiEngine
{
    class Scene;

    // Lua backend runtime. The sol2/Lua state is hidden behind Impl so the rest
    // of the engine never depends on third-party scripting headers.
    class LuaScriptRuntime final : public ScriptRuntime
    {
    public:
        explicit LuaScriptRuntime(Scene& scene);
        ~LuaScriptRuntime() override;

        void CreateInstance(Entity entity, uint32_t slotIndex, const std::filesystem::path& sourcePath,
                            const std::string& displayName) override;
        void Update(Timestep timestep, Scene& scene) override;
        void Shutdown(Scene& scene) override;

    private:
        struct Impl;
        Scope<Impl> m_Impl;
    };
}
