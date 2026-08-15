#pragma once

#include "Core/Base.h"
#include "Core/Log.h"

#if defined(_MSC_VER)
#define HE_DEBUG_BREAK() __debugbreak()
#else
#include <csignal>
#define HE_DEBUG_BREAK() std::raise(SIGTRAP)
#endif

#ifdef _DEBUG
#define HE_ENABLE_ASSERTS 1
#else
#define HE_ENABLE_ASSERTS 0
#endif

#define HE_CORE_ASSERT(condition)                                                              \
    do                                                                                         \
    {                                                                                          \
        if (!(condition))                                                                      \
        {                                                                                      \
            HE_CORE_CRITICAL("Assertion failed: {} in {}:{}", #condition, __FILE__, __LINE__); \
            if constexpr (HE_ENABLE_ASSERTS) HE_DEBUG_BREAK();                                 \
        }                                                                                      \
    } while (false)

#define HE_CLIENT_ASSERT(condition)                                                              \
    do                                                                                          \
    {                                                                                           \
        if (!(condition))                                                                       \
        {                                                                                       \
            HE_CLIENT_CRITICAL("Assertion failed: {} in {}:{}", #condition, __FILE__, __LINE__); \
            if constexpr (HE_ENABLE_ASSERTS) HE_DEBUG_BREAK();                                  \
        }                                                                                       \
    } while (false)
