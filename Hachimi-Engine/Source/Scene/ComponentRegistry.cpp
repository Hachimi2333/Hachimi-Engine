#include "Scene/ComponentRegistry.h"

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Scene/Components/CameraComponent.h"
#include "Scene/Components/ColliderComponent.h"
#include "Scene/Components/IDComponent.h"
#include "Scene/Components/LightComponent.h"
#include "Scene/Components/MeshComponent.h"
#include "Scene/Components/RelationshipComponent.h"
#include "Scene/Components/RigidbodyComponent.h"
#include "Scene/Components/ScriptComponent.h"
#include "Scene/Components/TagComponent.h"
#include "Scene/Components/TransformComponent.h"

namespace HachimiEngine
{
    namespace
    {
        std::vector<ComponentDescriptor>& Descriptors()
        {
            // Function-local so the table cannot be read before it exists, whichever executable
            // and whichever translation unit gets there first.
            static std::vector<ComponentDescriptor> descriptors;
            return descriptors;
        }

        bool& BuiltinsRegistered()
        {
            static bool registered = false;
            return registered;
        }
    }

    void ComponentRegistry::Register(const ComponentDescriptor& descriptor)
    {
        HE_CORE_ASSERT(descriptor.IsValid());
        HE_CORE_ASSERT(!descriptor.Name.empty());
        HE_CORE_ASSERT(Find(descriptor.TypeID) == nullptr);

        Descriptors().push_back(descriptor);
    }

    const std::vector<ComponentDescriptor>& ComponentRegistry::GetDescriptors()
    {
        EnsureBuiltinComponentsRegistered();
        return Descriptors();
    }

    const ComponentDescriptor* ComponentRegistry::Find(entt::id_type typeId)
    {
        for (const ComponentDescriptor& descriptor : Descriptors())
        {
            if (descriptor.TypeID == typeId)
            {
                return &descriptor;
            }
        }
        return nullptr;
    }

    void ComponentRegistry::EnsureBuiltinComponentsRegistered()
    {
        if (BuiltinsRegistered())
        {
            return;
        }

        BuiltinsRegistered() = true;
        RegisterBuiltinComponents();
    }

    void ComponentRegistry::RegisterBuiltinComponents()
    {
        // Registration order is the order used for serialization, duplication and the editor's
        // component list, so the three stay consistent by construction. Add a component here
        // and everywhere that walks the table picks it up.
        Register(MakeIDComponentDescriptor());
        Register(MakeTagComponentDescriptor());
        Register(MakeTransformComponentDescriptor());
        Register(MakeRelationshipComponentDescriptor());
        Register(MakeMeshComponentDescriptor());
        Register(MakeCameraComponentDescriptor());
        Register(MakeLightComponentDescriptor());
        Register(MakeRigidbodyComponentDescriptor());
        Register(MakeColliderComponentDescriptor());
        Register(MakeScriptComponentDescriptor());
    }
}
