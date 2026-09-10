-- zstd static library project.
-- Zstandard backs the .hpak game package container: every packed entry is split
-- into independently decompressible blocks. Hachimi-Engine additionally reuses
-- zstd's bundled xxHash (lib/common/xxhash.h) for path and content hashing, so
-- no separate hash library is vendored.
project "zstd"
    kind "StaticLib"
    language "C"
    warnings "Off"

    includedirs
    {
        "%{prj.location}/lib"
    }

    defines
    {
        -- Keep the library self-contained: the x86-64 BMI2 assembly sources are
        -- not vendored, and MSVC cannot assemble them anyway.
        "ZSTD_DISABLE_ASM"
    }

    files
    {
        "%{prj.location}/lib/*.h",
        "%{prj.location}/lib/common/*.c",
        "%{prj.location}/lib/common/*.h",
        "%{prj.location}/lib/compress/*.c",
        "%{prj.location}/lib/compress/*.h",
        "%{prj.location}/lib/decompress/*.c",
        "%{prj.location}/lib/decompress/*.h",
        "%{prj.location}/lib/dictBuilder/*.c",
        "%{prj.location}/lib/dictBuilder/*.h"
    }
