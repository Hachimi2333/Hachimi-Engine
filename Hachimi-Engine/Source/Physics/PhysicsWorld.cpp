#include "Physics/PhysicsWorld.h"

#include "Core/Log.h"
#include "Physics/PhysicsMath.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace HachimiEngine
{
    namespace
    {
        constexpr float MinShapeExtent = 0.01f;
        constexpr float MaxFrameDelta = 0.25f;

        b3BodyType ToBox3DBodyType(RigidbodyComponent::RigidbodyType type)
        {
            switch (type)
            {
                case RigidbodyComponent::RigidbodyType::Static:
                    return b3_staticBody;
                case RigidbodyComponent::RigidbodyType::Kinematic:
                    return b3_kinematicBody;
                case RigidbodyComponent::RigidbodyType::Dynamic:
                    return b3_dynamicBody;
            }

            return b3_staticBody;
        }

        Math::Vec3 MultiplyComponents(const Math::Vec3& lhs, const Math::Vec3& rhs)
        {
            return { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z };
        }

        bool NearlyEqual(const b3Vec3& lhs, const b3Vec3& rhs)
        {
            constexpr float epsilon = 1e-5f;
            return std::fabs(lhs.x - rhs.x) <= epsilon
                && std::fabs(lhs.y - rhs.y) <= epsilon
                && std::fabs(lhs.z - rhs.z) <= epsilon;
        }

        bool NearlyEqual(const b3Quat& lhs, const b3Quat& rhs)
        {
            // Quaternions double-cover orientation, so compare the absolute dot product.
            constexpr float epsilon = 1e-5f;
            const float dot = std::fabs(lhs.v.x * rhs.v.x + lhs.v.y * rhs.v.y + lhs.v.z * rhs.v.z + lhs.s * rhs.s);
            return dot >= 1.0f - epsilon;
        }

        // Decomposes a scene world matrix into a scale-free body transform and the
        // per-axis world scale. Physics body orientation must not contain scale.
        void DecomposeWorldTransform(
            const Math::Mat4& worldTransform,
            b3Vec3& outPosition,
            b3Quat& outRotation,
            Math::Vec3& outScale)
        {
            outScale = { Math::Length(worldTransform[0]), Math::Length(worldTransform[1]), Math::Length(worldTransform[2]) };

            Math::Mat3 rotationMatrix(worldTransform);
            for (int column = 0; column < 3; ++column)
            {
                const float scale = outScale[column];
                if (scale > 1e-6f)
                {
                    rotationMatrix[column] /= scale;
                }
            }

            const Math::Quat rotation = Math::QuatCast(rotationMatrix);
            outPosition = ToBox3D(Math::Vec3(worldTransform[3].x, worldTransform[3].y, worldTransform[3].z));
            outRotation = ToBox3D(rotation);
        }

        b3Vec3 ScaledOffset(const ColliderComponent& collider, const Math::Vec3& worldScale)
        {
            return ToBox3D(MultiplyComponents(collider.Offset, worldScale));
        }

        void ApplyBodySettings(b3BodyId bodyId, const RigidbodyComponent& rigidbody)
        {
            b3Body_SetType(bodyId, ToBox3DBodyType(rigidbody.Type));
            b3Body_SetLinearDamping(bodyId, rigidbody.LinearDamping);
            b3Body_SetAngularDamping(bodyId, rigidbody.AngularDamping);
            b3Body_SetGravityScale(bodyId, rigidbody.GravityScale);
            b3Body_EnableSleep(bodyId, rigidbody.EnableSleep);
            b3Body_SetBullet(bodyId, rigidbody.IsBullet);

            if (rigidbody.IsEnabled != b3Body_IsEnabled(bodyId))
            {
                if (rigidbody.IsEnabled)
                {
                    b3Body_Enable(bodyId);
                }
                else
                {
                    b3Body_Disable(bodyId);
                }
            }
        }

        b3BodyId CreateBody(
            b3WorldId worldId,
            const RigidbodyComponent& rigidbody,
            const ColliderComponent& collider,
            const Math::Mat4& worldTransform)
        {
            b3Vec3 bodyPosition;
            b3Quat bodyRotation;
            Math::Vec3 effectiveScale;
            DecomposeWorldTransform(worldTransform, bodyPosition, bodyRotation, effectiveScale);

            b3BodyDef bodyDef = b3DefaultBodyDef();
            bodyDef.type = ToBox3DBodyType(rigidbody.Type);
            bodyDef.position = bodyPosition;
            bodyDef.rotation = bodyRotation;
            bodyDef.linearVelocity = ToBox3D(rigidbody.LinearVelocity);
            bodyDef.angularVelocity = ToBox3D(rigidbody.AngularVelocity);
            bodyDef.linearDamping = rigidbody.LinearDamping;
            bodyDef.angularDamping = rigidbody.AngularDamping;
            bodyDef.gravityScale = rigidbody.GravityScale;
            bodyDef.enableSleep = rigidbody.EnableSleep;
            bodyDef.isAwake = rigidbody.InitiallyAwake;
            bodyDef.isBullet = rigidbody.IsBullet;
            bodyDef.isEnabled = rigidbody.IsEnabled;

            const b3BodyId bodyId = b3CreateBody(worldId, &bodyDef);
            if (B3_IS_NULL(bodyId))
            {
                HE_CORE_ERROR("Box3D failed to create a rigidbody");
                return b3_nullBodyId;
            }

            b3ShapeDef shapeDef = b3DefaultShapeDef();
            shapeDef.density = collider.Density;
            shapeDef.baseMaterial.friction = collider.Friction;
            shapeDef.baseMaterial.restitution = collider.Restitution;
            shapeDef.baseMaterial.rollingResistance = collider.RollingResistance;
            shapeDef.filter.categoryBits = collider.CategoryBits;
            shapeDef.filter.maskBits = collider.MaskBits;
            shapeDef.isSensor = collider.IsTrigger;
            shapeDef.enableSensorEvents = collider.IsTrigger;

            b3ShapeId shapeId = b3_nullShapeId;
            switch (collider.ShapeType)
            {
                case ColliderComponent::ColliderShapeType::Box:
                {
                    const Math::Vec3 halfExtents(
                        std::max(collider.HalfExtents.x * std::fabs(effectiveScale.x), MinShapeExtent),
                        std::max(collider.HalfExtents.y * std::fabs(effectiveScale.y), MinShapeExtent),
                        std::max(collider.HalfExtents.z * std::fabs(effectiveScale.z), MinShapeExtent));

                    b3BoxHull boxHull = b3MakeTransformedBoxHull(
                        halfExtents.x,
                        halfExtents.y,
                        halfExtents.z,
                        { ScaledOffset(collider, effectiveScale), b3Quat_identity });
                    shapeId = b3CreateHullShape(bodyId, &shapeDef, &boxHull.base);
                    break;
                }
                case ColliderComponent::ColliderShapeType::Sphere:
                {
                    const float radiusScale = std::max({
                        std::fabs(effectiveScale.x),
                        std::fabs(effectiveScale.y),
                        std::fabs(effectiveScale.z) });
                    b3Sphere sphere = {
                        ScaledOffset(collider, effectiveScale),
                        std::max(collider.Radius * radiusScale, MinShapeExtent) };
                    shapeId = b3CreateSphereShape(bodyId, &shapeDef, &sphere);
                    break;
                }
                case ColliderComponent::ColliderShapeType::Capsule:
                {
                    const float radiusScale = std::max(std::fabs(effectiveScale.x), std::fabs(effectiveScale.z));
                    const float radius = std::max(collider.Radius * radiusScale, MinShapeExtent);
                    const float halfLength = std::max(
                        (collider.Height * 0.5f - collider.Radius) * std::fabs(effectiveScale.y),
                        radius * 0.25f);

                    const b3Vec3 offset = ScaledOffset(collider, effectiveScale);
                    b3Capsule capsule = {
                        { offset.x, offset.y - halfLength, offset.z },
                        { offset.x, offset.y + halfLength, offset.z },
                        radius };
                    shapeId = b3CreateCapsuleShape(bodyId, &shapeDef, &capsule);
                    break;
                }
                case ColliderComponent::ColliderShapeType::Plane:
                {
                    // Box3D has no plane primitive; a thin static box is equivalent
                    // for the current editor floor/ground use cases.
                    constexpr float halfThickness = 0.05f;
                    const Math::Vec3 halfExtents(
                        std::max(collider.HalfExtents.x * std::fabs(effectiveScale.x), MinShapeExtent),
                        halfThickness,
                        std::max(collider.HalfExtents.z * std::fabs(effectiveScale.z), MinShapeExtent));

                    b3Vec3 offset = ScaledOffset(collider, effectiveScale);
                    offset.y -= halfThickness;

                    b3BoxHull planeHull = b3MakeTransformedBoxHull(
                        halfExtents.x,
                        halfExtents.y,
                        halfExtents.z,
                        { offset, b3Quat_identity });
                    shapeId = b3CreateHullShape(bodyId, &shapeDef, &planeHull.base);
                    break;
                }
            }

            if (B3_IS_NULL(shapeId))
            {
                HE_CORE_ERROR("Box3D failed to create a collider shape");
                b3DestroyBody(bodyId);
                return b3_nullBodyId;
            }

            return bodyId;
        }

        Math::Vec3 DecomposePosition(const Math::Mat4& matrix)
        {
            return { matrix[3].x, matrix[3].y, matrix[3].z };
        }

        Math::Vec3 DecomposeEulerRadians(const Math::Mat4& matrix)
        {
            Math::Mat3 rotationMatrix(matrix);
            for (int column = 0; column < 3; ++column)
            {
                const float scale = Math::Length(rotationMatrix[column]);
                if (scale > 1e-6f)
                {
                    rotationMatrix[column] /= scale;
                }
            }

            return Math::EulerAngles(Math::QuatCast(rotationMatrix));
        }
    }

    struct PhysicsWorld::Impl
    {
        explicit Impl(const PhysicsSettings& settings)
            : Settings(settings)
        {
        }

        PhysicsSettings Settings;
        b3WorldId WorldId = b3_nullWorldId;
        float Accumulator = 0.0f;
        std::unordered_map<uint64_t, b3BodyId> Bodies;
    };

    PhysicsWorld::PhysicsWorld(const PhysicsSettings& settings)
        : m_Impl(CreateScope<Impl>(settings))
    {
        b3WorldDef worldDef = b3DefaultWorldDef();
        worldDef.gravity = ToBox3D(settings.Gravity);
        worldDef.enableSleep = settings.EnableSleep;
        worldDef.enableContinuous = settings.EnableContinuous;

        m_Impl->WorldId = b3CreateWorld(&worldDef);
        if (B3_IS_NULL(m_Impl->WorldId))
        {
            HE_CORE_ERROR("Box3D failed to create a physics world");
            return;
        }

        HE_CORE_INFO(
            "Box3D physics world created (gravity = [{}, {}, {}], fixed step = {:.4f}s, sub-steps = {})",
            settings.Gravity.x,
            settings.Gravity.y,
            settings.Gravity.z,
            settings.FixedTimeStep,
            settings.SubStepCount);
    }

    PhysicsWorld::~PhysicsWorld()
    {
        if (m_Impl == nullptr)
        {
            return;
        }

        if (B3_IS_NON_NULL(m_Impl->WorldId))
        {
            b3DestroyWorld(m_Impl->WorldId);
            m_Impl->WorldId = b3_nullWorldId;
            HE_CORE_INFO("Box3D physics world destroyed");
        }
    }

    void PhysicsWorld::CreateBodies(Scene& scene)
    {
        if (m_Impl == nullptr || B3_IS_NULL(m_Impl->WorldId))
        {
            return;
        }

        auto view = scene.GetRegistry().view<RigidbodyComponent, TransformComponent>();
        for (const entt::entity entity : view)
        {
            const uint64_t entityKey = static_cast<uint64_t>(entt::to_integral(entity));
            if (m_Impl->Bodies.contains(entityKey))
            {
                continue;
            }

            const Entity sceneEntity(entity, &scene);
            if (!sceneEntity.HasComponent<ColliderComponent>())
            {
                HE_CORE_WARN(
                    "Skipping rigidbody '{}' because it has no collider component",
                    sceneEntity.GetName());
                continue;
            }

            const auto& rigidbody = view.get<RigidbodyComponent>(entity);
            const auto& collider = sceneEntity.GetComponent<ColliderComponent>();
            const b3BodyId bodyId = CreateBody(
                m_Impl->WorldId,
                rigidbody,
                collider,
                scene.GetWorldTransform(entity));

            if (B3_IS_NON_NULL(bodyId))
            {
                m_Impl->Bodies[entityKey] = bodyId;
            }
        }
    }

    void PhysicsWorld::Update(Scene& scene, Timestep timestep)
    {
        if (m_Impl == nullptr || B3_IS_NULL(m_Impl->WorldId))
        {
            return;
        }

        // Create new bodies, update shared settings, and destroy bodies whose
        // entities were removed from the registry since the last update.
        std::unordered_set<uint64_t> currentEntityKeys;
        auto view = scene.GetRegistry().view<RigidbodyComponent, TransformComponent>();
        for (const entt::entity entity : view)
        {
            const uint64_t entityKey = static_cast<uint64_t>(entt::to_integral(entity));
            currentEntityKeys.insert(entityKey);

            const Entity sceneEntity(entity, &scene);
            if (!sceneEntity.HasComponent<ColliderComponent>())
            {
                // A collider removal is the only component-level removal that can
                // leave an orphaned Box3D body behind after the first update.
                const auto orphanIt = m_Impl->Bodies.find(entityKey);
                if (orphanIt != m_Impl->Bodies.end())
                {
                    b3DestroyBody(orphanIt->second);
                    m_Impl->Bodies.erase(orphanIt);
                }
                continue;
            }

            const auto& rigidbody = view.get<RigidbodyComponent>(entity);
            const auto& collider = sceneEntity.GetComponent<ColliderComponent>();

            auto bodyIt = m_Impl->Bodies.find(entityKey);
            if (bodyIt == m_Impl->Bodies.end())
            {
                const b3BodyId bodyId = CreateBody(
                    m_Impl->WorldId,
                    rigidbody,
                    collider,
                    scene.GetWorldTransform(entity));
                if (B3_IS_NULL(bodyId))
                {
                    continue;
                }
                m_Impl->Bodies[entityKey] = bodyId;
                continue;
            }

            const b3BodyId bodyId = bodyIt->second;
            const b3BodyType previousType = b3Body_GetType(bodyId);
            ApplyBodySettings(bodyId, rigidbody);

            // Static and kinematic bodies are driven by the scene transform.
            // Dynamic body velocity is set only on creation or type changes so
            // gravity and impulses can accumulate normally between frames.
            if (rigidbody.Type != RigidbodyComponent::RigidbodyType::Dynamic || previousType != b3_dynamicBody)
            {
                b3Body_SetLinearVelocity(bodyId, ToBox3D(rigidbody.LinearVelocity));
                b3Body_SetAngularVelocity(bodyId, ToBox3D(rigidbody.AngularVelocity));
            }

            if (rigidbody.Type != RigidbodyComponent::RigidbodyType::Dynamic)
            {
                b3Vec3 bodyPosition;
                b3Quat bodyRotation;
                Math::Vec3 effectiveScale;
                DecomposeWorldTransform(
                    scene.GetWorldTransform(entity),
                    bodyPosition,
                    bodyRotation,
                    effectiveScale);

                // Only teleport when the authored transform actually changed;
                // repeated SetTransform calls would degrade broad-phase quality.
                const b3Transform currentTransform = b3Body_GetTransform(bodyId);
                if (!NearlyEqual(currentTransform.p, bodyPosition) || !NearlyEqual(currentTransform.q, bodyRotation))
                {
                    b3Body_SetTransform(bodyId, bodyPosition, bodyRotation);
                }
            }
        }

        for (auto bodyIt = m_Impl->Bodies.begin(); bodyIt != m_Impl->Bodies.end();)
        {
            if (currentEntityKeys.contains(bodyIt->first))
            {
                ++bodyIt;
                continue;
            }

            b3DestroyBody(bodyIt->second);
            bodyIt = m_Impl->Bodies.erase(bodyIt);
        }

        const float frameTime = std::min(timestep.GetSeconds(), MaxFrameDelta);
        m_Impl->Accumulator = std::min(m_Impl->Accumulator + frameTime, m_Impl->Settings.FixedTimeStep * 4.0f);

        const float fixedTimeStep = std::max(m_Impl->Settings.FixedTimeStep, 1e-4f);
        const int subStepCount = std::max(m_Impl->Settings.SubStepCount, 1);
        while (m_Impl->Accumulator >= fixedTimeStep)
        {
            b3World_Step(m_Impl->WorldId, fixedTimeStep, subStepCount);
            m_Impl->Accumulator -= fixedTimeStep;
        }

        // Write simulated world transforms back into the ECS. Scale stays
        // editor-authored because rigid bodies do not change scale.
        for (const entt::entity entity : view)
        {
            const auto& rigidbody = view.get<RigidbodyComponent>(entity);
            if (rigidbody.Type != RigidbodyComponent::RigidbodyType::Dynamic)
            {
                continue;
            }

            const uint64_t entityKey = static_cast<uint64_t>(entt::to_integral(entity));
            const auto bodyIt = m_Impl->Bodies.find(entityKey);
            if (bodyIt == m_Impl->Bodies.end())
            {
                continue;
            }

            const b3Transform bodyTransform = b3Body_GetTransform(bodyIt->second);
            Math::Mat4 worldTransform = ToMath(bodyTransform);

            auto& transform = view.get<TransformComponent>(entity);
            if (const auto* relationship = scene.GetRegistry().try_get<RelationshipComponent>(entity);
                relationship != nullptr && relationship->Parent != UUID::Invalid())
            {
                const Entity parent = scene.GetEntityByUUID(relationship->Parent);
                if (parent)
                {
                    const Math::Mat4 parentWorld = scene.GetWorldTransform(parent.GetHandle());
                    worldTransform = Math::Inverse(parentWorld) * worldTransform;
                }
            }

            transform.Position = DecomposePosition(worldTransform);
            transform.Rotation = Math::Degrees(DecomposeEulerRadians(worldTransform));
        }
    }

    void PhysicsWorld::DestroyBody(uint64_t entityKey)
    {
        if (m_Impl == nullptr)
        {
            return;
        }

        const auto bodyIt = m_Impl->Bodies.find(entityKey);
        if (bodyIt == m_Impl->Bodies.end())
        {
            return;
        }

        b3DestroyBody(bodyIt->second);
        m_Impl->Bodies.erase(bodyIt);
    }

    bool PhysicsWorld::IsRunning() const
    {
        return m_Impl != nullptr && B3_IS_NON_NULL(m_Impl->WorldId);
    }
}
