#pragma once

// Thin math wrapper around GLM. Engine and editor code must use
// HachimiEngine::Math types and functions instead of including GLM directly.

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

namespace HachimiEngine
{
    namespace Math
    {
        using Vec2 = glm::vec2;
        using Vec3 = glm::vec3;
        using Vec4 = glm::vec4;
        using Mat3 = glm::mat3;
        using Mat4 = glm::mat4;
        using Quat = glm::quat;

        template<typename T>
        inline T Pi()
        {
            return glm::pi<T>();
        }

        template<typename T>
        inline T TwoPi()
        {
            return glm::two_pi<T>();
        }

        template<typename T>
        inline T Radians(T value)
        {
            return glm::radians(value);
        }

        template<typename T>
        inline T Degrees(T value)
        {
            return glm::degrees(value);
        }

        template<typename T>
        inline auto Length(const T& value)
        {
            return glm::length(value);
        }

        template<typename T>
        inline T Normalize(const T& value)
        {
            return glm::normalize(value);
        }

        template<typename T, typename U>
        inline auto Dot(const T& lhs, const U& rhs)
        {
            return glm::dot(lhs, rhs);
        }

        template<typename T, typename U>
        inline auto Cross(const T& lhs, const U& rhs)
        {
            return glm::cross(lhs, rhs);
        }

        template<typename T>
        inline T Min(const T& lhs, const T& rhs)
        {
            return (glm::min)(lhs, rhs);
        }

        template<typename T>
        inline T Max(const T& lhs, const T& rhs)
        {
            return (glm::max)(lhs, rhs);
        }

        template<typename T>
        inline T Clamp(const T& value, const T& minValue, const T& maxValue)
        {
            return (glm::clamp)(value, minValue, maxValue);
        }

        template<typename T, typename U>
        inline auto Mix(const T& lhs, const T& rhs, U factor)
        {
            return glm::mix(lhs, rhs, factor);
        }

        inline Vec3 Rotate(const Quat& rotation, const Vec3& vector)
        {
            return glm::rotate(rotation, vector);
        }

        inline Quat QuatCast(const Mat3& matrix)
        {
            return glm::quat_cast(matrix);
        }

        inline Vec3 EulerAngles(const Quat& rotation)
        {
            return glm::eulerAngles(rotation);
        }

        inline Mat4 ToMat4(const Quat& rotation)
        {
            return glm::toMat4(rotation);
        }

        inline Mat4 Translate(const Mat4& matrix, const Vec3& translation)
        {
            return glm::translate(matrix, translation);
        }

        inline Mat4 Scale(const Mat4& matrix, const Vec3& scale)
        {
            return glm::scale(matrix, scale);
        }

        inline Mat4 LookAt(const Vec3& eye, const Vec3& center, const Vec3& up)
        {
            return glm::lookAt(eye, center, up);
        }

        inline Mat4 Perspective(float fieldOfView, float aspectRatio, float nearClip, float farClip)
        {
            return glm::perspective(fieldOfView, aspectRatio, nearClip, farClip);
        }

        inline Mat4 Ortho(float left, float right, float bottom, float top, float nearClip, float farClip)
        {
            return glm::ortho(left, right, bottom, top, nearClip, farClip);
        }

        template<typename T>
        inline T Inverse(const T& value)
        {
            return glm::inverse(value);
        }

        template<typename T>
        inline T Transpose(const T& value)
        {
            return glm::transpose(value);
        }

        template<typename T>
        inline auto ValuePtr(T& value)
        {
            return glm::value_ptr(value);
        }

        template<typename T>
        inline auto ValuePtr(const T& value)
        {
            return glm::value_ptr(value);
        }
    }
}
