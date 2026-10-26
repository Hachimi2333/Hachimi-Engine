#pragma once

#include "Scene/ComponentRegistry.h"
#include "Math/Math.h"

namespace HachimiEngine
{
    struct RigidbodyComponent
    {
        enum class RigidbodyType
        {
            Static = 0,
            Kinematic = 1,
            Dynamic = 2
        };

        RigidbodyType Type = RigidbodyType::Dynamic;
        Math::Vec3 LinearVelocity { 0.0f };
        Math::Vec3 AngularVelocity { 0.0f };
        float LinearDamping = 0.0f;
        float AngularDamping = 0.0f;
        float GravityScale = 1.0f;
        bool EnableSleep = true;
        bool InitiallyAwake = true;
        bool IsBullet = false;
        bool IsEnabled = true;

        // Where the body was before the last fixed step, in the same local space as
        // TransformComponent. Rendering blends the two by the frame's interpolation alpha, so a
        // body simulated at 60 Hz is drawn smoothly on a 144 Hz display instead of repeating each
        // simulated position.
        //
        // Runtime-only: these are neither serialized nor cloned. A body that has never been stepped
        // has PreviousTransformValid false, and rendering falls back to the authored transform.
        Math::Vec3 PreviousPosition { 0.0f };
        Math::Vec3 PreviousRotation { 0.0f };
        bool PreviousTransformValid = false;
    };

    ComponentDescriptor MakeRigidbodyComponentDescriptor();
}
