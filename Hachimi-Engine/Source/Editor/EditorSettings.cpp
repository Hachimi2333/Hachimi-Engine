#include "Editor/EditorSettings.h"

#include "Utils/FileSystem.h"
#include "Utils/PlatformUtils.h"

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iterator>
#include <cstdlib>


namespace HachimiEngine
{
    namespace
    {
        template<typename TSettings, typename TVisit>
        void VisitPanels(TSettings& settings, TVisit visit)
        {
            auto& panels = settings.Panels;
            visit("Hierarchy", panels.Hierarchy);
            visit("Inspector", panels.Inspector);
            visit("ContentBrowser", panels.ContentBrowser);
            visit("Viewport", panels.Viewport);
            visit("Game", panels.Game);
            visit("Console", panels.Console);
            visit("PackageInspector", panels.PackageInspector);
            visit("SceneSettings", panels.SceneSettings);
        }
    }

    std::filesystem::path EditorSettings::GetPath()
    {
        // Never fall back to the process working directory for personal settings.
        const char* appData = std::getenv("APPDATA");
        if (appData == nullptr || *appData == '\0') return {};
        const auto path = PlatformUtils::GetApplicationDataDirectory();
        if (!path.is_absolute()) return {};
        return path / "EditorSettings.yaml";
    }

    std::string EditorSettings::Serialize() const
    {
        YAML::Emitter out;
        out << YAML::BeginMap << YAML::Key << "Version" << YAML::Value << 1;
        out << YAML::Key << "Layout" << YAML::Value << YAML::Literal << Layout;
        out << YAML::Key << "Panels" << YAML::Value << YAML::BeginMap;
        VisitPanels(*this, [&](const char* name, bool visible) { out << YAML::Key << name << YAML::Value << visible; });
        out << YAML::EndMap << YAML::EndMap;
        return out.c_str();
    }

    bool EditorSettings::Deserialize(std::string_view text, EditorSettings& out)
    {
        out = {};
        try
        {
            const auto node = YAML::Load(std::string(text));
            if (!node.IsMap() || node["Version"].as<int>(0) != 1) return false;
            EditorSettings settings;
            settings.Layout = node["Layout"].as<std::string>("");
            if (!settings.Layout.empty() && settings.Layout.find("[Docking][Data]") == std::string::npos)
                return false;
            const auto panels = node["Panels"];
            if (panels && !panels.IsMap()) return false;
            VisitPanels(settings, [&](const char* name, bool& visible)
            {
                if (panels && panels[name]) visible = panels[name].as<bool>();
            });
            out = std::move(settings);
            return true;
        }
        catch (const YAML::Exception&) { return false; }
    }

    bool EditorSettings::Load(const std::filesystem::path& path)
    {
        *this = {};
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return Deserialize(text, *this);
    }

    bool EditorSettings::Save(const std::filesystem::path& path) const
    {
        if (path.empty() || !path.is_absolute() || !FileSystem::CreateDirectories(path.parent_path())) return false;
        return FileSystem::WriteTextFileAtomic(path, Serialize());
    }
}
