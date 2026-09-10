#include "Packaging/PackageFormat.h"

#include "Core/Log.h"
#include "Utils/XXHash.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <cstddef>

namespace HachimiEngine
{
    uint64_t ComputePackageTocHash(const void* tocData, size_t tocSize)
    {
        constexpr size_t hashFieldOffset = offsetof(PackageTocHeader, TocContentHash);
        constexpr size_t hashFieldSize = sizeof(uint64_t);

        if (tocData == nullptr || tocSize < hashFieldOffset + hashFieldSize)
        {
            return 0;
        }

        const uint8_t* bytes = static_cast<const uint8_t*>(tocData);
        XXHashStream stream;
        stream.Update(bytes, hashFieldOffset);
        stream.Update(bytes + hashFieldOffset + hashFieldSize,
            tocSize - hashFieldOffset - hashFieldSize);
        return stream.Digest();
    }

    bool ParsePackageBuildInfo(const std::string& yamlText, PackageBuildInfo& outBuildInfo)
    {
        outBuildInfo = {};

        YAML::Node data;
        try
        {
            data = YAML::Load(yamlText);
        }
        catch (const YAML::Exception& exception)
        {
            HE_CORE_ERROR("Failed to parse BuildInfo.yaml: {}", exception.what());
            return false;
        }

        if (!data)
        {
            HE_CORE_ERROR("Failed to parse BuildInfo.yaml: empty document");
            return false;
        }

        outBuildInfo.ProductName = data["ProductName"].as<std::string>("");
        outBuildInfo.StartScene = data["StartScene"].as<std::string>("");
        outBuildInfo.WindowWidth = data["WindowWidth"].as<uint32_t>(1600);
        outBuildInfo.WindowHeight = data["WindowHeight"].as<uint32_t>(900);
        outBuildInfo.VSync = data["VSync"].as<bool>(true);
        return outBuildInfo.IsValid();
    }

    std::string MakePackageEntryName(const std::filesystem::path& path)
    {
        if (path.empty() || path.is_absolute() || path.has_root_name() || path.has_root_directory())
        {
            return {};
        }

        std::string entryName = path.generic_string();
        std::replace(entryName.begin(), entryName.end(), '\\', '/');

        // Drop any leading "./" so lookups and listings agree on one spelling.
        while (entryName.starts_with("./"))
        {
            entryName.erase(0, 2);
        }

        return IsSafePackageEntryName(entryName) ? entryName : std::string{};
    }

    bool IsSafePackageEntryName(std::string_view entryName)
    {
        if (entryName.empty() || entryName.size() > 4096)
        {
            return false;
        }

        if (entryName.front() == '/' || entryName.front() == '\\')
        {
            return false;
        }

        // A trailing separator would create an empty final component.
        if (entryName.back() == '/' || entryName.back() == '\\')
        {
            return false;
        }

        // Reject Windows drive-relative forms such as "C:/x".
        if (entryName.size() >= 2 && entryName[1] == ':')
        {
            return false;
        }

        size_t componentBegin = 0;
        while (componentBegin <= entryName.size())
        {
            const size_t separator = entryName.find_first_of("/\\", componentBegin);
            const std::string_view component = separator == std::string_view::npos
                ? entryName.substr(componentBegin)
                : entryName.substr(componentBegin, separator - componentBegin);

            if (component == "..")
            {
                return false;
            }
            if (component.empty() && separator != std::string_view::npos)
            {
                // Reject "a//b"; the writer never produces it and it would make
                // directory matching ambiguous.
                return false;
            }

            if (separator == std::string_view::npos)
            {
                break;
            }
            componentBegin = separator + 1;
        }

        return true;
    }

    std::string ToLowerAscii(std::string_view text)
    {
        std::string lowered(text);
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
        return lowered;
    }

    std::string GetPackageEntryDirectory(std::string_view entryName)
    {
        const size_t separator = entryName.find_last_of('/');
        if (separator == std::string_view::npos)
        {
            return {};
        }
        return std::string(entryName.substr(0, separator));
    }
}
