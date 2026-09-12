#include "Support/TestWorkspace.h"

#include "Packaging/PackageWriter.h"
#include "Utils/FileSystem.h"
#include "Utils/VirtualFileSystem.h"

#include <algorithm>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>

namespace HachimiEngine
{
    namespace Tests
    {
        namespace
        {
            constexpr uint32_t TestCompressionLevel = 3;

            std::unique_ptr<TestWorkspace> g_Workspace;

            std::filesystem::path MakeWorkspaceRoot()
            {
                std::error_code errorCode;
                std::filesystem::path root = std::filesystem::temp_directory_path(errorCode);
                if (errorCode)
                {
                    root = std::filesystem::current_path();
                }
                return root / "HachimiTests";
            }
        }

        TestWorkspace::TestWorkspace()
            : m_Root(MakeWorkspaceRoot())
            , m_SampleRoot(m_Root / "samples")
            , m_PackagePath(m_Root / "Test.hpak")
        {
            FileSystem::RemoveAll(m_Root);
            FileSystem::CreateDirectories(m_Root);

            m_BuildInfoYaml =
                "ProductName: RoundTrip\n"
                "StartScene: Data/multi_block.bin\n"
                "WindowWidth: 800\n"
                "WindowHeight: 600\n"
                "VSync: true\n";

            BuildSampleTree();

            PackageWriterSettings settings;
            settings.BlockSize = TestBlockSize;
            settings.CompressionLevel = TestCompressionLevel;
            settings.UseDictionary = true;
            settings.MultiThreaded = true;

            PackageWriter writer;
            if (!writer.Open(m_PackagePath, settings))
            {
                throw std::runtime_error("TestWorkspace: cannot open " + m_PackagePath.string() + " for writing");
            }

            for (const Sample& sample : m_Samples)
            {
                if (!writer.AddFile(sample.VirtualPath, sample.SourcePath))
                {
                    throw std::runtime_error("TestWorkspace: cannot add " + sample.VirtualPath);
                }
            }

            if (!writer.AddMemory("BuildInfo.yaml", m_BuildInfoYaml.data(), m_BuildInfoYaml.size()))
            {
                throw std::runtime_error("TestWorkspace: cannot add BuildInfo.yaml");
            }

            PackageBuildReport report;
            if (!writer.Finalize(report))
            {
                throw std::runtime_error("TestWorkspace: cannot finalize " + m_PackagePath.string());
            }
        }

        TestWorkspace::~TestWorkspace()
        {
            FileSystem::RemoveAll(m_Root);
        }

        void TestWorkspace::BuildSampleTree()
        {
            FileSystem::CreateDirectories(m_SampleRoot);

            const auto add = [this](const std::string& virtualPath, std::vector<uint8_t> content, bool expectStored)
            {
                Sample sample;
                sample.VirtualPath = virtualPath;
                sample.Content = std::move(content);
                sample.SourcePath = m_SampleRoot / std::filesystem::path(virtualPath);
                sample.ExpectStored = expectStored;
                FileSystem::CreateDirectories(sample.SourcePath.parent_path());
                WriteBytes(sample.SourcePath, sample.Content);
                m_Samples.push_back(std::move(sample));
            };

            // Boundary cases around the block size drive the segmentation logic.
            add("Data/empty.bin", {}, false);
            add("Data/one_byte.bin", { 0x42 }, false);
            add("Data/exactly_one_block.bin", MakeCompressibleBytes(TestBlockSize), false);
            add("Data/block_plus_one.bin", MakeCompressibleBytes(TestBlockSize + 1), false);
            add(MultiBlockEntry, MakeCompressibleBytes(TestBlockSize * 5 + 7), false);

            // Incompressible data must fall back to stored blocks inside a zstd entry.
            add(RandomEntry, MakeRandomBytes(TestBlockSize * 4 + 3, 1337), false);
            // An already-compressed extension is stored outright.
            add(StoredEntry, MakeRandomBytes(TestBlockSize * 2 + 11, 99), true);

            // Spaces and mixed case exercise path handling and lookup normalization.
            add("Assets/Meshes/Shared Meshes/Box Mesh.bin", MakeCompressibleBytes(1024), false);

            // Enough small text samples for the dictionary trainer to be exercised.
            for (size_t index = 0; index < ScriptSampleCount; ++index)
            {
                const std::string name = std::string(ScriptsEntryDirectory) + "/script_"
                    + std::to_string(index) + ".lua";
                const std::string text = "local M = {}\nfunction M:OnCreate()\n  HE.Log.Info(\"script "
                    + std::to_string(index) + "\")\nend\nreturn M\n";
                add(name, std::vector<uint8_t>(text.begin(), text.end()), false);
            }
        }

        const Sample* TestWorkspace::FindSample(const std::string& virtualPath) const
        {
            for (const Sample& sample : m_Samples)
            {
                if (sample.VirtualPath == virtualPath)
                {
                    return &sample;
                }
            }
            return nullptr;
        }

        std::filesystem::path TestWorkspace::PrepareDirectory(const std::string& relativeName) const
        {
            const std::filesystem::path directory = m_Root / relativeName;
            FileSystem::RemoveAll(directory);
            FileSystem::CreateDirectories(directory);
            return directory;
        }

        ScopedArchiveMount::ScopedArchiveMount(int priority)
        {
            const TestWorkspace& workspace = Workspace();
            m_MountPoint = workspace.PackagePath().parent_path();

            VirtualFileSystem::UnmountAll();
            m_Mounted = VirtualFileSystem::MountArchive(workspace.PackagePath(), m_MountPoint, priority);
        }

        ScopedArchiveMount::~ScopedArchiveMount()
        {
            VirtualFileSystem::UnmountAll();
        }

        const TestWorkspace& Workspace()
        {
            if (g_Workspace == nullptr)
            {
                g_Workspace = std::make_unique<TestWorkspace>();
            }
            return *g_Workspace;
        }

        void DestroyWorkspace()
        {
            VirtualFileSystem::UnmountAll();
            g_Workspace.reset();
        }

        std::filesystem::path WritePlayerStandIn(const std::filesystem::path& directory)
        {
            FileSystem::CreateDirectories(directory);

            const std::vector<uint8_t> bytes = MakeRandomBytes(2048, 4242);
            const std::filesystem::path path = directory / "PlayerStandIn.exe";
            WriteBytes(path, bytes);
            return path;
        }

        size_t ExpectedBlockCount(size_t size)
        {
            return size == 0 ? 0 : (size + TestBlockSize - 1) / TestBlockSize;
        }

        std::vector<uint8_t> MakeRandomBytes(size_t size, uint32_t seed)
        {
            std::mt19937 generator(seed);
            std::uniform_int_distribution<int> distribution(0, 255);

            std::vector<uint8_t> bytes(size);
            for (uint8_t& byte : bytes)
            {
                byte = static_cast<uint8_t>(distribution(generator));
            }
            return bytes;
        }

        std::vector<uint8_t> MakeCompressibleBytes(size_t size)
        {
            const std::string pattern = "Hachimi-Engine asset payload; zstd compresses this very well.\n";
            std::vector<uint8_t> bytes;
            bytes.reserve(size);
            while (bytes.size() < size)
            {
                const size_t chunk = std::min(pattern.size(), size - bytes.size());
                bytes.insert(bytes.end(), pattern.begin(), pattern.begin() + static_cast<ptrdiff_t>(chunk));
            }
            return bytes;
        }

        bool WriteBytes(const std::filesystem::path& path, const std::vector<uint8_t>& data)
        {
            return FileSystem::WriteBinaryFile(path, data.data(), data.size());
        }

        bool SameBytes(const std::vector<uint8_t>& lhs, const std::vector<uint8_t>& rhs)
        {
            return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
        }

        bool SameSpan(const std::vector<uint8_t>& lhs, const std::vector<uint8_t>& rhs, size_t rhsOffset)
        {
            return rhsOffset + lhs.size() <= rhs.size()
                && std::equal(lhs.begin(), lhs.end(), rhs.begin() + static_cast<ptrdiff_t>(rhsOffset));
        }
    }
}
