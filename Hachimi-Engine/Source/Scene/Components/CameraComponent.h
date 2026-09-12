#pragma once

#include "Scene/ComponentRegistry.h"

namespace HachimiEngine
{
    struct CameraComponent
    {
        bool Primary = false;
        float FieldOfView = 45.0f;
        float NearClip = 0.1f;
        float FarClip = 1000.0f;
    };

    ComponentDescriptor MakeCameraComponentDescriptor();
}
