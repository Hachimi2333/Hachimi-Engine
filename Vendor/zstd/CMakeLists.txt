# zstd (Zstandard) static library.
#
# zstd ships a complete CMake project, but it lives in the non-standard build/cmake/
# subdirectory rather than next to this file. This forwarder is the only thing owned by
# Hachimi-Engine here, and no zstd source file is modified.
#
# The upstream project defines the `zstd::libzstd_static` target used by the engine's
# package writer/reader.

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/build/cmake")
