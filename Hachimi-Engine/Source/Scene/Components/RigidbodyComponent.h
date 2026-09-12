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
    };

    ComponentDescriptor MakeRigidbodyComponentDescriptor();
}
