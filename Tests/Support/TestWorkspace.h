#pragma once

#include "Core/Base.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// Shared fixture for every suite: one sample tree on disk plus the .hpak written from it.
//
// Lifetime rules the suites rely on:
//   - main() calls DestroyWorkspace() after the doctest run, so a workspace never outlives
//     the engine systems it was built with.
//   - A test case must not call JobSystem::Shutdown(): main() owns the engine lifetime and
//     other suites still need the worker pool afterwards.
//   - Every test case owns the mount it creates: use ScopedArchiveMount or unmount inside
//     the case. Nothing may depend on the order the cases run in.
namespace HachimiEngine
{
    namespace Tests
    {
        // One synthetic asset: its path inside the package and the bytes it has to round trip.
        struct Sample
        {
            std::string VirtualPath;
            std::filesystem::path SourcePath;
            std::vector<uint8_t> Content;
            bool ExpectStored = false;
        };

        // Package layout the packaging and virtual file system suites share.
        inline constexpr uint32_t TestBlockSize = 4096;
        inline constexpr const char* MultiBlockEntry = "Data/multi_block.bin";
        inline constexpr const char* RandomEntry = "Data/random.bin";
        inline constexpr const char* StoredEntry = "Assets/Textures/random.png";
        inline constexpr const char* ScriptsEntryDirectory = "Assets/Scripts";
        inline constexpr size_t ScriptSampleCount = 12;

        class TestWorkspace
        {
        public:
            // Builds <temp>/HachimiTests from scratch: the sample tree and Test.hpak.
            TestWorkspace();
            ~TestWorkspace();

            TestWorkspace(const TestWorkspace&) = delete;
            TestWorkspace& operator=(const TestWorkspace&) = delete;

            // <temp>/HachimiTests, wiped and recreated when the workspace is built.
            const std::filesystem::path& Root() const { return m_Root; }
            // The loose files the package was written from.
            const std::filesystem::path& SampleRoot() const { return m_SampleRoot; }
            // The package every sample was written into, including BuildInfo.yaml.
            const std::filesystem::path& PackagePath() const { return m_PackagePath; }
            const std::vector<Sample>& Samples() const { return m_Samples; }
            const std::string& BuildInfoYaml() const { return m_BuildInfoYaml; }

            const Sample* FindSample(const std::string& virtualPath) const;

            // Removes and recreates a directory below the workspace root.
            std::filesystem::path PrepareDirectory(const std::string& relativeName) const;

        private:
            void BuildSampleTree();

            std::filesystem::path m_Root;
            std::filesystem::path m_SampleRoot;
            std::filesystem::path m_PackagePath;
            std::string m_BuildInfoYaml;
            std::vector<Sample> m_Samples;
        };

        // Mounts the shared package at its own directory and drops every mount again on
        // destruction, so an aborted test case cannot leak a mount into the next one.
        class ScopedArchiveMount
        {
        public:
            explicit ScopedArchiveMount(int priority = 0);
            ~ScopedArchiveMount();

            ScopedArchiveMount(const ScopedArchiveMount&) = delete;
            ScopedArchiveMount& operator=(const ScopedArchiveMount&) = delete;

            bool IsMounted() const { return m_Mounted; }
            const std::filesystem::path& MountPoint() const { return m_MountPoint; }

        private:
            std::filesystem::path m_MountPoint;
            bool m_Mounted = false;
        };

        // Created on first use, destroyed by DestroyWorkspace().
        const TestWorkspace& Workspace();
        void DestroyWorkspace();

        // A file that stands in for Hachimi-Player.exe in the export tests: ProjectPackager
        // only has to find and copy it, so no real build of the Player is needed.
        std::filesystem::path WritePlayerStandIn(const std::filesystem::path& directory);

        size_t ExpectedBlockCount(size_t size);
        std::vector<uint8_t> MakeRandomBytes(size_t size, uint32_t seed);
        std::vector<uint8_t> MakeCompressibleBytes(size_t size);
        bool WriteBytes(const std::filesystem::path& path, const std::vector<uint8_t>& data);
        bool SameBytes(const std::vector<uint8_t>& lhs, const std::vector<uint8_t>& rhs);
        bool SameSpan(const std::vector<uint8_t>& lhs, const std::vector<uint8_t>& rhs, size_t rhsOffset);
    }
}
