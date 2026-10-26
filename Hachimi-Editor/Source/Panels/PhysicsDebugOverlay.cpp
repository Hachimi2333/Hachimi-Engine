#include "Panels/PhysicsDebugOverlay.h"

#include "Panels/EditorContext.h"
#include "Physics/PhysicsWorld.h"
#include "Renderer/DebugDraw.h"
#include "Scene/Components/ColliderComponent.h"
#include "Scene/Components/RigidbodyComponent.h"
#include "Scene/Components/TransformComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/Systems/PhysicsSystem.h"

#include <algorithm>
#include <cmath>

namespace HachimiEngine
{
    namespace
    {
        // The same states Box3D colours when it draws a world, so a collider does not change
        // appearance the moment play starts. A sleeping body is the one exception: sleep only exists
        // while simulating, and there the world's own colours are used.
        const Math::Vec4 StaticColliderColor { 0.55f, 0.55f, 0.58f, 1.0f };
        const Math::Vec4 KinematicColliderColor { 0.27f, 0.51f, 0.71f, 1.0f };
        const Math::Vec4 DynamicColliderColor { 0.82f, 0.71f, 0.55f, 1.0f };
        const Math::Vec4 TriggerColliderColor { 0.96f, 0.87f, 0.70f, 1.0f };
        const Math::Vec4 DisabledColliderColor { 0.44f, 0.50f, 0.56f, 1.0f };

        // A plane collider is a thin box in the simulation (Box3D has no plane primitive), and the
        // wireframe mirrors that so the drawn shape is the shape that collides.
        constexpr float PlaneHalfThickness = 0.05f;

        PhysicsWorld* WorldOf(Scene& scene)
        {
            PhysicsSystem* physics = scene.FindSystem<PhysicsSystem>();
            return physics != nullptr ? physics->GetWorld() : nullptr;
        }

        Math::Vec4 ColliderColor(const RigidbodyComponent& rigidbody, const ColliderComponent& collider)
        {
            if (!rigidbody.IsEnabled)
            {
                return DisabledColliderColor;
            }

            if (collider.IsTrigger)
            {
                return TriggerColliderColor;
            }

            switch (rigidbody.Type)
            {
                case RigidbodyComponent::RigidbodyType::Static:
                    return StaticColliderColor;
                case RigidbodyComponent::RigidbodyType::Kinematic:
                    return KinematicColliderColor;
                case RigidbodyComponent::RigidbodyType::Dynamic:
                    return DynamicColliderColor;
            }

            return DynamicColliderColor;
        }

        // The radius and half length the simulation derives from a capsule's radius and total height.
        void CapsuleExtents(const ColliderComponent& collider, const Math::Vec3& scale, float& outRadius, float& outHalfLength)
        {
            const float radiusScale = std::max(std::fabs(scale.x), std::fabs(scale.z));
            outRadius = collider.Radius * radiusScale;
            outHalfLength = std::max(
                (collider.Height * 0.5f - collider.Radius) * std::fabs(scale.y),
                outRadius * 0.25f);
        }
    }

    void PhysicsDebugOverlay::Draw(EditorContext& context, DebugDraw& debugDraw, const Math::Mat4& viewProjection)
    {
        if (context.ActiveScene == nullptr
            || (!context.ShowPhysicsColliders && !context.ShowPhysicsBounds && !context.ShowPhysicsContacts))
        {
            return;
        }

        m_Target = &debugDraw;
        m_Target->Begin(viewProjection);

        PhysicsWorld* world = WorldOf(*context.ActiveScene);
        if (context.IsPlaying() && world != nullptr)
        {
            PhysicsDebugDrawOptions options;
            options.Shapes = context.ShowPhysicsColliders;
            options.Bounds = context.ShowPhysicsBounds;
            options.Contacts = context.ShowPhysicsContacts;
            world->DrawDebug(*this, options);
        }
        else if (context.ShowPhysicsColliders)
        {
            DrawAuthoredColliders(*context.ActiveScene);
        }

        m_Target->End();
        m_Target = nullptr;
    }

    void PhysicsDebugOverlay::DrawAuthoredColliders(Scene& scene)
    {
        auto view = scene.GetRegistry().view<RigidbodyComponent, ColliderComponent, TransformComponent>();
        for (const entt::entity entity : view)
        {
            const auto& rigidbody = view.get<RigidbodyComponent>(entity);
            const auto& collider = view.get<ColliderComponent>(entity);
            const Math::Mat4 worldTransform = scene.GetWorldTransform(entity);

            const Math::Vec3 position = Math::DecomposePosition(worldTransform);
            const Math::Quat rotation = Math::DecomposeRotation(worldTransform);
            const Math::Vec3 scale = Math::DecomposeScale(worldTransform);
            const Math::Vec4 color = ColliderColor(rigidbody, collider);

            // The collider's own offset is in the entity's space scaled with it, which is the same
            // arithmetic the body creation does before handing the shape to Box3D.
            const Math::Vec3 center = position
                + Math::Rotate(rotation, Math::Vec3(
                    collider.Offset.x * scale.x,
                    collider.Offset.y * scale.y,
                    collider.Offset.z * scale.z));

            switch (collider.ShapeType)
            {
                case ColliderComponent::ColliderShapeType::Box:
                {
                    const Math::Vec3 halfExtents(
                        collider.HalfExtents.x * std::fabs(scale.x),
                        collider.HalfExtents.y * std::fabs(scale.y),
                        collider.HalfExtents.z * std::fabs(scale.z));
                    Box(halfExtents, center, rotation, color);
                    break;
                }
                case ColliderComponent::ColliderShapeType::Sphere:
                {
                    const float radiusScale = std::max({
                        std::fabs(scale.x),
                        std::fabs(scale.y),
                        std::fabs(scale.z) });
                    Sphere(center, collider.Radius * radiusScale, color);
                    break;
                }
                case ColliderComponent::ColliderShapeType::Capsule:
                {
                    float radius = 0.0f;
                    float halfLength = 0.0f;
                    CapsuleExtents(collider, scale, radius, halfLength);

                    const Math::Vec3 axis = Math::Rotate(rotation, Math::Vec3(0.0f, halfLength, 0.0f));
                    Capsule(center - axis, center + axis, radius, color);
                    break;
                }
                case ColliderComponent::ColliderShapeType::Plane:
                {
                    const Math::Vec3 halfExtents(
                        collider.HalfExtents.x * std::fabs(scale.x),
                        PlaneHalfThickness,
                        collider.HalfExtents.z * std::fabs(scale.z));
                    const Math::Vec3 offset = Math::Rotate(rotation, Math::Vec3(0.0f, -PlaneHalfThickness, 0.0f));
                    Box(halfExtents, center + offset, rotation, color);
                    break;
                }
            }
        }
    }

    void PhysicsDebugOverlay::Segment(const Math::Vec3& start, const Math::Vec3& end, const Math::Vec4& color)
    {
        if (m_Target != nullptr)
        {
            m_Target->DrawLine(start, end, color);
        }
    }

    void PhysicsDebugOverlay::Point(const Math::Vec3& position, float extent, const Math::Vec4& color)
    {
        if (m_Target != nullptr)
        {
            m_Target->DrawPoint(position, extent, color);
        }
    }

    void PhysicsDebugOverlay::Sphere(const Math::Vec3& center, float radius, const Math::Vec4& color)
    {
        if (m_Target != nullptr)
        {
            m_Target->DrawSphere(center, radius, color);
        }
    }

    void PhysicsDebugOverlay::Capsule(const Math::Vec3& start, const Math::Vec3& end, float radius, const Math::Vec4& color)
    {
        if (m_Target != nullptr)
        {
            m_Target->DrawCapsule(start, end, radius, color);
        }
    }

    void PhysicsDebugOverlay::Box(
        const Math::Vec3& halfExtents,
        const Math::Vec3& center,
        const Math::Quat& rotation,
        const Math::Vec4& color)
    {
        if (m_Target != nullptr)
        {
            m_Target->DrawBox(halfExtents, center, rotation, color);
        }
    }

    void PhysicsDebugOverlay::Bounds(const Math::Vec3& minimum, const Math::Vec3& maximum, const Math::Vec4& color)
    {
        if (m_Target == nullptr)
        {
            return;
        }

        // An AABB is a box with no rotation, so the debug renderer's oriented box draws it as is.
        const Math::Vec3 center = (minimum + maximum) * 0.5f;
        const Math::Vec3 halfExtents = (maximum - minimum) * 0.5f;
        m_Target->DrawBox(halfExtents, center, Math::Quat(1.0f, 0.0f, 0.0f, 0.0f), color);
    }
}
