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

// Assertion that stays active in Release builds.
//
// HE_CORE_ASSERT only breaks into the debugger under _DEBUG, which is right for "this cannot
// happen" checks but wrong for preconditions whose violation is undefined behaviour rather than
// a wrong result: Entity::GetComponent and the hierarchy walk both rely on theirs. Those use
// these macros, so a violated precondition stops the program in every configuration instead of
// corrupting memory quietly.
#define HE_CORE_VERIFY(condition)                                                              \
    do                                                                                         \
    {                                                                                          \
        if (!(condition))                                                                      \
        {                                                                                      \
            HE_CORE_CRITICAL("Precondition failed: {} in {}:{}", #condition, __FILE__, __LINE__); \
            HE_DEBUG_BREAK();                                                                  \
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

#define HE_CLIENT_VERIFY(condition)                                                              \
    do                                                                                          \
    {                                                                                           \
        if (!(condition))                                                                       \
        {                                                                                       \
            HE_CLIENT_CRITICAL("Precondition failed: {} in {}:{}", #condition, __FILE__, __LINE__); \
            HE_DEBUG_BREAK();                                                                   \
        }                                                                                       \
    } while (false)
