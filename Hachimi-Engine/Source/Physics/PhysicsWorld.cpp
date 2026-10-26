#include "Physics/PhysicsWorld.h"

#include "Core/Log.h"
#include "Physics/PhysicsBodyKey.h"
#include "Physics/PhysicsMath.h"
#include "Renderer/PhysicsDebugDraw.h"
#include "Scene/Components/ColliderComponent.h"
#include "Scene/Components/IDComponent.h"
#include "Scene/Components/RelationshipComponent.h"
#include "Scene/Components/RigidbodyComponent.h"
#include "Scene/Components/TransformComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace HachimiEngine
{
    // Box3D hands its sub-tasks to whatever the world definition points at, so the backend's C
    // callback shape is bridged here - the only place in the engine that has both headers. The
    // scheduler comes in as the world's user task context and stays free of third-party types.
    //
    // C linkage because these are stored in Box3D's function pointers, which are declared in a C
    // header: matching the language linkage keeps the assignment exactly the type Box3D declares
    // instead of relying on a compiler treating the two as interchangeable.
    extern "C" void* PhysicsEnqueueTaskBridge(
        b3TaskCallback* task,
        void* taskContext,
        void* userContext,
        const char* taskName)
    {
        // Box3D's task name is a diagnostic label for a profiler; the scheduler has nowhere to
        // report one, so it is deliberately dropped here.
        (void)taskName;

        auto* scheduler = static_cast<PhysicsTaskScheduler*>(userContext);
        if (scheduler == nullptr || task == nullptr)
        {
            return nullptr;
        }

        return scheduler->Enqueue(task, taskContext);
    }

    extern "C" void PhysicsFinishTaskBridge(void* userTask, void* userContext)
    {
        auto* scheduler = static_cast<PhysicsTaskScheduler*>(userContext);
        if (scheduler == nullptr)
        {
            return;
        }

        scheduler->Finish(userTask);
    }

    static_assert(
        PhysicsTaskScheduler::MaxWorkers == B3_MAX_WORKERS,
        "the scheduler's worker limit must match the count Box3D accepts");

    namespace
    {
        constexpr float MinShapeExtent = 0.01f;

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

        Math::Vec3 DecomposeEulerRadians(const Math::Mat4& matrix)
        {
            return Math::EulerAngles(Math::DecomposeRotation(matrix));
        }

        // Decomposes a scene world matrix into a scale-free body transform and the
        // per-axis world scale.
        void DecomposeWorldTransform(
            const Math::Mat4& worldTransform,
            b3Vec3& outPosition,
            b3Quat& outRotation,
            Math::Vec3& outScale)
        {
            outScale = Math::DecomposeScale(worldTransform);
            outPosition = ToBox3D(Math::DecomposePosition(worldTransform));
            outRotation = ToBox3D(Math::DecomposeRotation(worldTransform));
        }

        b3Vec3 ScaledOffset(const ColliderComponent& collider, const Math::Vec3& worldScale)
        {
            return ToBox3D(MultiplyComponents(collider.Offset, worldScale));
        }

        // Pushes every authorable body property into Box3D.
        //
        // Box3D takes these both as fields of the body definition and through setters, and the setter
        // is the one that matters for a body that already exists: the definition is only read by
        // b3CreateBody, so gravity scale set to zero in a test or in the inspector while the scene
        // runs would be ignored and the body would keep falling under full gravity.
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

        // Turns a shape's event reporting on or off.
        //
        // Box3D stores these flags per shape, not per world, and a pair reports an event when either
        // side asked for it - so a visitor that never asked is invisible to a trigger it walks into,
        // and every shape has to be told rather than the world.
        void ApplyEventSettings(b3ShapeId shapeId, bool isSensor, bool contactEvents, bool sensorEvents)
        {
            // A sensor is reported through the sensor channel, and Box3D ignores the contact flags for
            // one anyway. A sensor also does not need to be a visitor of its own channel: what the
            // caller cares about is the solid things inside it, and a pair of two volume triggers is
            // noise.
            b3Shape_EnableContactEvents(shapeId, contactEvents && !isSensor);
            b3Shape_EnableHitEvents(shapeId, contactEvents && !isSensor);
            b3Shape_EnableSensorEvents(shapeId, isSensor || sensorEvents);
        }

        // ------------------------------------------------------------ debug drawing
        //
        // Box3D draws a shape by asking the application for an opaque "user shape" once and then
        // handing that pointer back on every later frame together with the body transform, so the
        // application owns the geometry and the backend owns its lifetime. That is the only path
        // that draws shape geometry at all - the primitive callbacks in b3DebugDraw are used by the
        // recording player, not by b3World_Draw - which is why this adapter keeps its own record per
        // drawn shape instead of relying on the callback set alone.

        // World positions may be double precision in a large-world build, so the conversion is
        // explicit rather than an overload of the float one.
        Math::Vec3 ToMathPosition(const b3Pos& value)
        {
            return {
                static_cast<float>(value.x),
                static_cast<float>(value.y),
                static_cast<float>(value.z)
            };
        }

        // What one drawn shape reduces to once the renderer is line based. The geometry is copied at
        // creation because the pointers Box3D offers then are only valid at that moment.
        struct DebugShapeRecord
        {
            enum class Kind
            {
                Box,
                Sphere,
                Capsule
            };

            Kind ShapeKind = Kind::Box;
            // All in the shape's own space, so a body transform places them and an offset collider
            // keeps its offset.
            Math::Vec3 Center { 0.0f };
            Math::Vec3 HalfExtents { 0.0f };
            float Radius = 0.0f;
            Math::Vec3 Start { 0.0f };
            Math::Vec3 End { 0.0f };
        };

        // Owns the records. Box3D destroys each one when the shape it belongs to changes or dies;
        // the set is what keeps a world torn down with records still alive from leaking them.
        struct DebugShapeRegistry
        {
            ~DebugShapeRegistry()
            {
                for (const DebugShapeRecord* record : Records)
                {
                    delete record;
                }
            }

            std::unordered_set<DebugShapeRecord*> Records;
        };

        void* CreateDebugShape(const b3DebugShape* shape, void* userContext)
        {
            auto* registry = static_cast<DebugShapeRegistry*>(userContext);
            if (shape == nullptr || registry == nullptr)
            {
                return nullptr;
            }

            auto record = CreateScope<DebugShapeRecord>();

            switch (shape->type)
            {
                case b3_sphereShape:
                {
                    record->ShapeKind = DebugShapeRecord::Kind::Sphere;
                    record->Center = ToMath(shape->sphere->center);
                    record->Radius = shape->sphere->radius;
                    break;
                }
                case b3_capsuleShape:
                {
                    record->ShapeKind = DebugShapeRecord::Kind::Capsule;
                    record->Start = ToMath(shape->capsule->center1);
                    record->End = ToMath(shape->capsule->center2);
                    record->Radius = shape->capsule->radius;
                    break;
                }
                case b3_hullShape:
                {
                    // A hull has no primitive of its own, so it is drawn as its bounding box in
                    // shape space - which for the boxes and planes this engine creates is exactly
                    // the shape, because their geometry is axis aligned there.
                    const b3AABB bounds = b3ComputeHullAABB(shape->hull, b3Transform_identity);
                    record->ShapeKind = DebugShapeRecord::Kind::Box;
                    record->Center = ToMath(b3AABB_Center(bounds));
                    record->HalfExtents = ToMath(b3AABB_Extents(bounds));
                    break;
                }
                default:
                {
                    // Mesh, height field and compound shapes are not something a collider component
                    // can produce yet. Returning nothing leaves them undrawn, which is honest;
                    // guessing at their extent would draw a box that is not there.
                    return nullptr;
                }
            }

            registry->Records.insert(record.get());
            return record.release();
        }

        void DestroyDebugShape(void* userShape, void* userContext)
        {
            auto* registry = static_cast<DebugShapeRegistry*>(userContext);
            auto* record = static_cast<DebugShapeRecord*>(userShape);
            if (registry == nullptr || record == nullptr)
            {
                return;
            }

            registry->Records.erase(record);
            delete record;
        }

        // Draws one record at the body transform Box3D reports for it.
        bool DrawDebugShape(void* userShape, b3WorldTransform transform, b3HexColor color, void* context)
        {
            auto* sink = static_cast<PhysicsDebugSink*>(context);
            const auto* record = static_cast<const DebugShapeRecord*>(userShape);
            if (sink == nullptr || record == nullptr)
            {
                return true;
            }

            const Math::Vec3 position = ToMathPosition(transform.p);
            const Math::Quat rotation = ToMath(transform.q);
            const Math::Vec4 rgba = DecodePhysicsDebugColor(static_cast<uint32_t>(color));

            switch (record->ShapeKind)
            {
                case DebugShapeRecord::Kind::Box:
                    sink->Box(record->HalfExtents, position + Math::Rotate(rotation, record->Center), rotation, rgba);
                    break;
                case DebugShapeRecord::Kind::Sphere:
                    sink->Sphere(position + Math::Rotate(rotation, record->Center), record->Radius, rgba);
                    break;
                case DebugShapeRecord::Kind::Capsule:
                    sink->Capsule(
                        position + Math::Rotate(rotation, record->Start),
                        position + Math::Rotate(rotation, record->End),
                        record->Radius,
                        rgba);
                    break;
            }

            // Returning false would tell Box3D to stop drawing this shape; there is no such thing as
            // a shape this renderer cannot express.
            return true;
        }

        void DrawDebugSegment(b3Pos p1, b3Pos p2, b3HexColor color, void* context)
        {
            auto* sink = static_cast<PhysicsDebugSink*>(context);
            if (sink == nullptr)
            {
                return;
            }

            sink->Segment(ToMathPosition(p1), ToMathPosition(p2), DecodePhysicsDebugColor(static_cast<uint32_t>(color)));
        }

        void DrawDebugPoint(b3Pos p, float size, b3HexColor color, void* context)
        {
            auto* sink = static_cast<PhysicsDebugSink*>(context);
            if (sink == nullptr)
            {
                return;
            }

            sink->Point(
                ToMathPosition(p),
                PhysicsDebugPointExtent(size),
                DecodePhysicsDebugColor(static_cast<uint32_t>(color)));
        }

        void DrawDebugSphere(b3Pos p, float radius, b3HexColor color, float alpha, void* context)
        {
            auto* sink = static_cast<PhysicsDebugSink*>(context);
            if (sink == nullptr)
            {
                return;
            }

            sink->Sphere(ToMathPosition(p), radius, DecodePhysicsDebugColor(static_cast<uint32_t>(color), alpha));
        }

        void DrawDebugCapsule(b3Pos p1, b3Pos p2, float radius, b3HexColor color, float alpha, void* context)
        {
            auto* sink = static_cast<PhysicsDebugSink*>(context);
            if (sink == nullptr)
            {
                return;
            }

            sink->Capsule(
                ToMathPosition(p1),
                ToMathPosition(p2),
                radius,
                DecodePhysicsDebugColor(static_cast<uint32_t>(color), alpha));
        }

        void DrawDebugBounds(b3AABB aabb, b3HexColor color, void* context)
        {
            auto* sink = static_cast<PhysicsDebugSink*>(context);
            if (sink == nullptr)
            {
                return;
            }

            sink->Bounds(
                ToMath(aabb.lowerBound),
                ToMath(aabb.upperBound),
                DecodePhysicsDebugColor(static_cast<uint32_t>(color)));
        }

        void DrawDebugBox(b3Vec3 extents, b3WorldTransform transform, b3HexColor color, void* context)
        {
            auto* sink = static_cast<PhysicsDebugSink*>(context);
            if (sink == nullptr)
            {
                return;
            }

            sink->Box(
                ToMath(extents),
                ToMathPosition(transform.p),
                ToMath(transform.q),
                DecodePhysicsDebugColor(static_cast<uint32_t>(color)));
        }

        b3BodyId CreateBody(
            b3WorldId worldId,
            const RigidbodyComponent& rigidbody,
            const ColliderComponent& collider,
            const Math::Mat4& worldTransform,
            bool contactEventsEnabled,
            bool sensorEventsEnabled)        {
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
            shapeDef.enableSensorEvents = sensorEventsEnabled;
            shapeDef.enableContactEvents = contactEventsEnabled && !collider.IsTrigger;

            // Hit events ride along with contact events. Box3D asks for the flag separately and
            // reports a hit whenever a contact confirms an impulse above its speed threshold, so
            // "these two met" and "they met this hard" arrive in the same batch - which is the pair a
            // listener needs to tell a step onto a platform from a fall onto one.
            shapeDef.enableHitEvents = shapeDef.enableContactEvents;
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

            // Re-applied through the runtime helper rather than trusted to the definition above: the
            // two paths have to agree, and the shape is where Box3D actually reads them from.
            ApplyEventSettings(shapeId, collider.IsTrigger, contactEventsEnabled, sensorEventsEnabled);

            return bodyId;
        }

        // The owning entity of a shape, carried in the body's user data.
        //
        // Destroying a body clears it, so a shape id that outlived its entity is dropped by the query
        // and event paths instead of being read as a live key.
        uint64_t EntityKeyForShape(b3ShapeId shapeId)
        {
            if (!b3Shape_IsValid(shapeId))
            {
                return InvalidPhysicsEntityKey;
            }

            const b3BodyId bodyId = b3Shape_GetBody(shapeId);
            if (B3_IS_NULL(bodyId))
            {
                return InvalidPhysicsEntityKey;
            }

            const void* userData = b3Body_GetUserData(bodyId);
            return userData != nullptr ? DecodePhysicsBodyKey(userData) : InvalidPhysicsEntityKey;
        }

        // The sensor side of a sensor touch event, which is the entity that owns the trigger volume
        // rather than the one that walked into it.
        //
        // Box3D publishes sensor events by walking its own sensor array, so the sensor shape of both
        // a begin and an end event is alive and its body still carries the key. The visitor is the
        // side a destroyed body can take with it, and it is the caller that decides what to do when
        // its key decodes to 0.
        uint64_t SensorEntityKeyOf(b3ShapeId sensorShapeId)
        {
            return b3Shape_IsSensor(sensorShapeId) ? EntityKeyForShape(sensorShapeId) : InvalidPhysicsEntityKey;
        }

        bool ToBox3D(const PhysicsQueryFilter& filter, b3QueryFilter& outFilter)
        {
            outFilter = b3DefaultQueryFilter();
            outFilter.categoryBits = filter.CategoryBits;
            outFilter.maskBits = filter.MaskBits;

            // A query that cannot collide with anything is almost always a mistake - a mask of zero
            // reads as "hit nothing" rather than "no filter" - and Box3D would answer it with an
            // empty result the caller then has to debug. Refusing it here leaves the bug at the
            // call site instead of at the symptom.
            return filter.CategoryBits != 0 && filter.MaskBits != 0;
        }

        bool IsExcluded(uint64_t entityKey, const PhysicsQueryFilter& filter)
        {
            return entityKey == filter.ExcludeEntityKey;
        }

        // Hit being built by a ray or shape-cast callback.
        struct CastHit
        {
            PhysicsQueryFilter Filter;
            bool Valid = false;
            uint64_t EntityKey = 0;
            float Fraction = 1.0f;
            Math::Vec3 Point { 0.0f };
            Math::Vec3 Normal { 0.0f };
        };

        float RayCastCallback(
            b3ShapeId shapeId,
            b3Pos point,
            b3Vec3 normal,
            float fraction,
            uint64_t userMaterialId,
            int triangleIndex,
            int childIndex,
            void* context)
        {
            (void)userMaterialId;
            (void)triangleIndex;
            (void)childIndex;

            auto& hit = *static_cast<CastHit*>(context);
            const uint64_t entityKey = EntityKeyForShape(shapeId);
            if (entityKey == InvalidPhysicsEntityKey || IsExcluded(entityKey, hit.Filter))
            {
                // An ownerless shape - a body whose entity was destroyed - is not a hit anybody can
                // act on, so the ray passes through it rather than reporting an unusable result.
                return -1.0f;
            }

            if (hit.Valid && fraction >= hit.Fraction)
            {
                return hit.Fraction;
            }

            hit.Valid = true;
            hit.EntityKey = entityKey;
            hit.Fraction = fraction;
            hit.Point = ToMath(point);
            hit.Normal = ToMath(normal);
            return fraction;
        }

        // Same query, but collects instead of clipping, so the caller gets every shape along the ray
        // rather than only the closest one.
        struct RayCollectContext
        {
            PhysicsQueryFilter Filter;
            b3Vec3 Translation { 0.0f, 0.0f, 0.0f };
            RayCastResults* Results = nullptr;
            size_t MaxResults = 0;
        };

        float RayCollectCallback(
            b3ShapeId shapeId,
            b3Pos point,
            b3Vec3 normal,
            float fraction,
            uint64_t userMaterialId,
            int triangleIndex,
            int childIndex,
            void* context)
        {
            (void)userMaterialId;
            (void)triangleIndex;
            (void)childIndex;

            auto& collector = *static_cast<RayCollectContext*>(context);

            const uint64_t entityKey = EntityKeyForShape(shapeId);
            if (entityKey == InvalidPhysicsEntityKey || IsExcluded(entityKey, collector.Filter))
            {
                return -1.0f;
            }

            RayCastResult result;
            result.EntityKey = entityKey;
            result.Point = ToMath(point);
            result.Normal = ToMath(normal);
            // Returning a negative fraction means "not a hit, keep going", so every shape is
            // reported and the ray is never clipped to the first one found. The results come back in
            // whatever order the world happened to visit them.
            result.Distance = fraction * Math::Length(ToMath(collector.Translation));
            if (collector.Results->size() < collector.MaxResults)
                collector.Results->push_back(result);
            else
            {
                auto farthest = std::max_element(collector.Results->begin(), collector.Results->end(),
                    [](const auto& a, const auto& b) { return a.Distance < b.Distance; });
                if (result.Distance < farthest->Distance) *farthest = result;
            }
            if (collector.Results->size() < collector.MaxResults) return 1.0f;
            const auto farthest = std::max_element(collector.Results->begin(), collector.Results->end(),
                [](const auto& a, const auto& b) { return a.Distance < b.Distance; });
            return farthest->Distance / Math::Length(ToMath(collector.Translation));
        }

        struct OverlapCollectContext
        {
            PhysicsQueryFilter Filter;
            OverlapResults* Results = nullptr;
            size_t MaxResults = 0;
        };

        bool OverlapCallback(b3ShapeId shapeId, void* context)
        {
            auto& collector = *static_cast<OverlapCollectContext*>(context);

            if (collector.Results->size() >= collector.MaxResults)
            {
                return false;
            }

            const uint64_t entityKey = EntityKeyForShape(shapeId);
            if (entityKey == InvalidPhysicsEntityKey || IsExcluded(entityKey, collector.Filter))
            {
                return true;
            }

            const b3BodyId bodyId = b3Shape_GetBody(shapeId);

            OverlapResult result;
            result.EntityKey = entityKey;
            result.Position = ToMath(b3Body_GetPosition(bodyId));
            collector.Results->push_back(result);

            return true;
        }

        float ShapeCastCallback(
            b3ShapeId shapeId,
            b3Pos point,
            b3Vec3 normal,
            float fraction,
            uint64_t userMaterialId,
            int triangleIndex,
            int childIndex,
            void* context)
        {
            (void)userMaterialId;
            (void)triangleIndex;
            (void)childIndex;

            auto& hit = *static_cast<CastHit*>(context);

            const uint64_t entityKey = EntityKeyForShape(shapeId);
            if (entityKey == InvalidPhysicsEntityKey || IsExcluded(entityKey, hit.Filter))
            {
                return -1.0f;
            }

            if (hit.Valid && fraction >= hit.Fraction)
            {
                return hit.Fraction;
            }

            hit.Valid = true;
            hit.EntityKey = entityKey;
            hit.Fraction = fraction;
            hit.Point = ToMath(point);
            hit.Normal = ToMath(normal);

            // Fraction 0 means the shape already overlaps: there is nothing left to sweep, so the
            // query terminates instead of walking the rest of the world.
            return fraction;
        }

        // Box corners in the box's own space, for the overlap and cast proxies. A proxy is a point
        // cloud with a radius, so a box is its eight corners and a sphere is one point with the
        // sphere's radius.
        void FillBoxProxyPoints(const Math::Vec3& halfExtents, b3Vec3 (&outPoints)[8])
        {
            int index = 0;
            for (const float x : { -halfExtents.x, halfExtents.x })
            {
                for (const float y : { -halfExtents.y, halfExtents.y })
                {
                    for (const float z : { -halfExtents.z, halfExtents.z })
                    {
                        outPoints[index++] = b3Vec3 { x, y, z };
                    }
                }
            }
        }
    }

    struct PhysicsWorld::Impl
    {
        explicit Impl(const PhysicsSettings& settings)
            : Settings(settings)
            , Scheduler(CreateScope<PhysicsTaskScheduler>(settings.WorkerCount))
            , ContactEventsEnabled(settings.EnableContactEvents)
        {
        }

        PhysicsSettings Settings;
        b3WorldId WorldId = b3_nullWorldId;
        std::unordered_map<uint64_t, b3BodyId> Bodies;
        // Body id -> entity, so a query or an event that only hands back a shape can name its owner.
        std::unordered_map<uint64_t, uint64_t> BodyOwners;
        // Refilled by every step; see PhysicsWorld::GetEvents.
        PhysicsEventQueue Events;
        // Runs the step's sub-tasks and owns the rule that a step never runs on a pool worker.
        Scope<PhysicsTaskScheduler> Scheduler;
        // The geometry behind the debug shapes Box3D asks for while a debug view is drawn.
        Scope<DebugShapeRegistry> DebugShapes = CreateScope<DebugShapeRegistry>();
        // The worker count Box3D currently has, so a settings change is pushed once rather than asked
        // for on every step.
        uint32_t AppliedWorkerCount = 1;
        // The world-level settings last pushed into Box3D, for the same reason.
        Math::Vec3 AppliedGravity;
        bool AppliedEnableSleep = true;
        bool AppliedEnableContinuous = true;
        // The event flags last pushed into the shapes, so a runtime toggle only walks the shapes when
        // a setting actually changed rather than on every step.
        bool ContactEventsEnabled = true;
        bool SensorEventsEnabled = false;

        // Takes ownership of a freshly created body: the entity key travels in the body's user data,
        // which is what lets a query result or an event name its entity without a lookup table, and
        // the reverse map answers the same question for a shape id alone.
        void RegisterBody(uint64_t entityKey, b3BodyId bodyId)
        {
            b3Body_SetUserData(bodyId, EncodePhysicsBodyKey(entityKey));
            Bodies[entityKey] = bodyId;
            BodyOwners[b3StoreBodyId(bodyId)] = entityKey;
        }

        // Pushes the settings Box3D keeps on the world itself, and only the ones that moved.
        //
        // The editor edits a scene's settings while the scene runs, and the backend reads gravity and
        // the two enable flags only when they are set, so without this a change would wait for the
        // next play session even though every other settings edit takes effect immediately.
        void ApplyWorldSettings()
        {
            if (!NearlyEqual(ToBox3D(Settings.Gravity), ToBox3D(AppliedGravity)))
            {
                b3World_SetGravity(WorldId, ToBox3D(Settings.Gravity));
                AppliedGravity = Settings.Gravity;
            }

            if (Settings.EnableSleep != AppliedEnableSleep)
            {
                b3World_EnableSleeping(WorldId, Settings.EnableSleep);
                AppliedEnableSleep = Settings.EnableSleep;
            }

            if (Settings.EnableContinuous != AppliedEnableContinuous)
            {
                b3World_EnableContinuous(WorldId, Settings.EnableContinuous);
                AppliedEnableContinuous = Settings.EnableContinuous;
            }
        }

        // Releases a body and clears its user data before the id is recycled. Box3D hands back shape
        // ids of destroyed bodies for one more step through events and sensor overlaps, so a stale
        // id must read as "no owner" rather than as whatever entity inherits the slot next.
        void UnregisterBody(uint64_t entityKey)
        {
            const auto bodyIt = Bodies.find(entityKey);
            if (bodyIt == Bodies.end())
            {
                return;
            }

            b3Body_SetUserData(bodyIt->second, nullptr);
            BodyOwners.erase(b3StoreBodyId(bodyIt->second));
            b3DestroyBody(bodyIt->second);
            Bodies.erase(bodyIt);
        }

        // The body behind an entity key, or false when the entity has none. Every motion-state call
        // needs exactly this, and the accessors below stay one line each because of it.
        bool TryFindBody(uint64_t entityKey, b3BodyId& outBodyId) const        {
            if (B3_IS_NULL(WorldId))
            {
                return false;
            }

            const auto bodyIt = Bodies.find(entityKey);
            if (bodyIt == Bodies.end())
            {
                return false;
            }

            outBodyId = bodyIt->second;
            return true;
        }

        // The gate every event passes before it is published: an event that names an entity with no
        // body left would hand a caller a key that resolves to nothing, or - worse - to whatever
        // entity inherited the recycled slot. Dropping it here keeps that decision in one place.
        bool IsLiveEntity(uint64_t entityKey) const
        {
            return entityKey != InvalidPhysicsEntityKey && Bodies.contains(entityKey);
        }

        // Re-walks every shape when a scene-level event setting changed.
        //
        // Only called when a flag actually changed: doing it on every step would be a walk of every
        // body for nothing, and the flags only move when the editor or a script moves them.
        void RefreshEventSettings(bool contactEvents, bool sensorEvents)
        {
            if (contactEvents == ContactEventsEnabled && sensorEvents == SensorEventsEnabled)
            {
                return;
            }

            ContactEventsEnabled = contactEvents;
            SensorEventsEnabled = sensorEvents;

            std::vector<b3ShapeId> shapes;
            for (const auto& [entityKey, bodyId] : Bodies)
            {
                (void)entityKey;

                const int shapeCount = b3Body_GetShapeCount(bodyId);
                if (shapeCount <= 0)
                {
                    continue;
                }

                shapes.resize(static_cast<size_t>(shapeCount));
                const int written = b3Body_GetShapes(bodyId, shapes.data(), shapeCount);
                for (int index = 0; index < written; ++index)
                {
                    const b3ShapeId shapeId = shapes[static_cast<size_t>(index)];
                    ApplyEventSettings(shapeId, b3Shape_IsSensor(shapeId), contactEvents, sensorEvents);
                }
            }
        }

        void ToSceneCollision(const b3ContactBeginTouchEvent& event)
        {
            const uint64_t keyA = EntityKeyForShape(event.shapeIdA);
            const uint64_t keyB = EntityKeyForShape(event.shapeIdB);
            if (!IsLiveEntity(keyA) || !IsLiveEntity(keyB))
            {
                return;
            }

            CollisionBeginEvent begin;
            begin.EntityKeyA = keyA;
            begin.EntityKeyB = keyB;

            // The begin event carries a contact id, not the manifold, so the point and the normal are
            // read from the contact record. A manifold point is an offset from the centre of mass, and
            // a contact touching in several places has more than one, so the reported point is their
            // average: a listener wants "where roughly did these meet", and picking the first of four
            // would make the answer depend on manifold ordering.
            if (b3Contact_IsValid(event.contactId))
            {
                const b3ContactData contact = b3Contact_GetData(event.contactId);
                if (contact.manifoldCount > 0)
                {
                    b3BodyId bodyA;
                    b3BodyId bodyB;
                    const b3Manifold& manifold = contact.manifolds[0];
                    begin.Normal = ToMath(manifold.normal);

                    if (manifold.pointCount > 0 && TryFindBody(keyA, bodyA) && TryFindBody(keyB, bodyB))
                    {
                        // The midpoint of the two centres of mass, plus half of the two anchors. This
                        // is Box3D's own arithmetic for the point it reports on a hit event, and it is
                        // used here for the same reason: the two anchors are offsets in each body's
                        // own frame, so neither one alone describes a world point - their mean does
                        // once it is placed between the two bodies.
                        const b3Pos midCenter = b3LerpPosition(
                            b3Body_GetPosition(bodyA),
                            b3Body_GetPosition(bodyB),
                            0.5f);

                        b3Vec3 sum = b3Vec3_zero;
                        for (int point = 0; point < manifold.pointCount; ++point)
                        {
                            begin.Impulse += manifold.points[point].normalImpulse;
                            const b3Vec3 midAnchor = b3Lerp(
                                manifold.points[point].anchorA,
                                manifold.points[point].anchorB,
                                0.5f);
                            sum = b3Add(sum, b3OffsetPos(midCenter, midAnchor));
                        }

                        const float inverseCount = 1.0f / static_cast<float>(manifold.pointCount);
                        begin.Point = ToMath(b3MulSV(inverseCount, sum));
                    }
                }
            }

            Events.PushCollisionBegin(begin);
        }

        void ToSceneCollision(const b3ContactEndTouchEvent& event)
        {
            const uint64_t keyA = EntityKeyForShape(event.shapeIdA);
            const uint64_t keyB = EntityKeyForShape(event.shapeIdB);
            if (!IsLiveEntity(keyA) || !IsLiveEntity(keyB))
            {
                return;
            }

            CollisionEndEvent end;
            end.EntityKeyA = keyA;
            end.EntityKeyB = keyB;
            Events.PushCollisionEnd(end);
        }

        void ToSceneCollision(const b3ContactHitEvent& event)
        {
            const uint64_t keyA = EntityKeyForShape(event.shapeIdA);
            const uint64_t keyB = EntityKeyForShape(event.shapeIdB);
            if (!IsLiveEntity(keyA) || !IsLiveEntity(keyB))
            {
                return;
            }

            CollisionHitEvent hit;
            hit.EntityKeyA = keyA;
            hit.EntityKeyB = keyB;
            hit.Point = ToMath(event.point);
            hit.Normal = ToMath(event.normal);
            hit.ApproachSpeed = event.approachSpeed;
            Events.PushCollisionHit(hit);
        }

        // Whether any collider in the scene is a trigger.
        //
        // Polled instead of tracked with a counter, because a collider can be added, removed or
        // retyped between two updates and the walk happens once per step either way. The answer
        // decides whether the world asks Box3D to track sensor overlaps at all: a scene with no
        // trigger volumes has nothing to report through that channel, and asking for it would put
        // every shape through the sensor pass for nothing.
        bool HasTriggers(Scene& scene) const
        {
            auto view = scene.GetRegistry().view<ColliderComponent>();
            for (const entt::entity entity : view)
            {
                if (view.get<ColliderComponent>(entity).IsTrigger)
                {
                    return true;
                }
            }

            return false;
        }

        // Reads one step's worth of contact, sensor and body move events out of Box3D.
        //
        // The contact and sensor buffers are refilled by every b3World_Step, so this has to run
        // between the step and the next one - which is exactly where PhysicsWorld::Update calls it.
        // Body move events are handled here too because they are published the same way, and the
        // caller gets the moved bodies instead of having to ask every body for its transform.
        void CollectEvents()
        {
            Events.Clear();

            const b3ContactEvents contacts = b3World_GetContactEvents(WorldId);
            for (int index = 0; index < contacts.beginCount; ++index)
            {
                ToSceneCollision(contacts.beginEvents[index]);
            }
            for (int index = 0; index < contacts.endCount; ++index)
            {
                ToSceneCollision(contacts.endEvents[index]);
            }
            for (int index = 0; index < contacts.hitCount; ++index)
            {
                ToSceneCollision(contacts.hitEvents[index]);
            }

            // Sensor overlaps are reported per shape pair. A trigger volume can hold several shapes
            // per body once compound colliders exist, so the pair - not the body - is what Box3D
            // begins and ends; publishing both shapes' entities is what makes that an entity pair.
            const b3SensorEvents sensors = b3World_GetSensorEvents(WorldId);
            for (int index = 0; index < sensors.beginCount; ++index)
            {
                const uint64_t sensorKey = SensorEntityKeyOf(sensors.beginEvents[index].sensorShapeId);
                const uint64_t otherKey = EntityKeyForShape(sensors.beginEvents[index].visitorShapeId);
                if (!IsLiveEntity(sensorKey) || !IsLiveEntity(otherKey))
                {
                    continue;
                }

                TriggerBeginEvent begin;
                begin.SensorEntityKey = sensorKey;
                begin.OtherEntityKey = otherKey;
                Events.PushTriggerBegin(begin);
            }
            for (int index = 0; index < sensors.endCount; ++index)
            {
                const uint64_t sensorKey = SensorEntityKeyOf(sensors.endEvents[index].sensorShapeId);
                const uint64_t otherKey = EntityKeyForShape(sensors.endEvents[index].visitorShapeId);

                // An end event is allowed to name a body that is already gone - "the visitor was
                // destroyed while inside the trigger" is a real case - so requiring both keys to be
                // live would lose it. The trigger itself is what the listener subscribed for, and a
                // trigger only ends while it still exists.
                if (!IsLiveEntity(sensorKey))
                {
                    continue;
                }

                TriggerEndEvent end;
                end.SensorEntityKey = sensorKey;
                end.OtherEntityKey = otherKey;
                Events.PushTriggerEnd(end);
            }
        }
    };

    PhysicsWorld::PhysicsWorld(const PhysicsSettings& settings)
        : m_Impl(CreateScope<Impl>(settings))
    {
        b3WorldDef worldDef = b3DefaultWorldDef();
        worldDef.gravity = ToBox3D(settings.Gravity);
        worldDef.enableSleep = settings.EnableSleep;
        worldDef.enableContinuous = settings.EnableContinuous;

        // Box3D forks its step into tasks and blocks on each one, so the task callbacks are what turn
        // a step into parallel work. The scheduler resolves the requested worker count first: a
        // single worker means every task runs where Box3D asks for it, which is the backend's serial
        // mode rather than a thread hop per task.
        const uint32_t workerCount = m_Impl->Scheduler->SetWorkerCount(settings.WorkerCount);
        m_Impl->AppliedWorkerCount = workerCount;
        worldDef.workerCount = workerCount;
        worldDef.enqueueTask = &PhysicsEnqueueTaskBridge;
        worldDef.finishTask = &PhysicsFinishTaskBridge;
        worldDef.userTaskContext = m_Impl->Scheduler.get();

        // The debug shapes are created lazily by the first debug draw and owned here in between, so
        // they are wired up with the world rather than per draw call.
        worldDef.createDebugShape = &CreateDebugShape;
        worldDef.destroyDebugShape = &DestroyDebugShape;
        worldDef.userDebugShapeContext = m_Impl->DebugShapes.get();

        m_Impl->AppliedGravity = settings.Gravity;
        m_Impl->AppliedEnableSleep = settings.EnableSleep;
        m_Impl->AppliedEnableContinuous = settings.EnableContinuous;

        m_Impl->WorldId = b3CreateWorld(&worldDef);
        if (B3_IS_NULL(m_Impl->WorldId))
        {
            HE_CORE_ERROR("Box3D failed to create a physics world");
            return;
        }

        HE_CORE_INFO(
            "Box3D physics world created (gravity = [{}, {}, {}], fixed step = {:.4f}s, sub-steps = {}, "
            "workers = {}{})",
            settings.Gravity.x,
            settings.Gravity.y,
            settings.Gravity.z,
            settings.FixedTimeStep,
            settings.SubStepCount,
            workerCount,
            m_Impl->Scheduler->UsesThreadPool() ? " on the job system" : " (single threaded)");
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
                scene.GetWorldTransform(entity),
                m_Impl->ContactEventsEnabled,
                m_Impl->SensorEventsEnabled);

            if (B3_IS_NON_NULL(bodyId))
            {
                m_Impl->RegisterBody(entityKey, bodyId);
            }
        }
    }

    void PhysicsWorld::Update(Scene& scene, Timestep timestep)
    {
        if (m_Impl == nullptr || B3_IS_NULL(m_Impl->WorldId))
        {
            return;
        }

        // The step is the one place a world touches every thread it forked, and Box3D parks the
        // stepping thread on those tasks until they finish. On a pool worker that can starve the pool
        // of the very threads the step needs, so a step asked for from a job is refused here - before
        // anything is synchronized - rather than attempted and hung.
        if (!m_Impl->Scheduler->TryBeginStep())
        {
            return;
        }

        // The scene owns the settings, and the editor can change them while the scene runs, so the
        // copy the world steers by is refreshed here rather than only at construction. Sub-step count
        // and gravity are read from it on the next step; the contact-event flag is pushed into the
        // shapes below, because Box3D keeps it per shape.
        m_Impl->Settings = scene.GetPhysicsSettings();

        // A worker count is applied to the live world: Box3D rebuilds its worker contexts for one, so
        // an inspector edit takes effect on the next step instead of needing a Play/Stop cycle. A
        // single worker means the step runs entirely on this thread, which is what the scheduler
        // already assumes when it hands tasks back inline.
        const uint32_t workerCount = m_Impl->Scheduler->SetWorkerCount(m_Impl->Settings.WorkerCount);
        if (workerCount != m_Impl->AppliedWorkerCount)
        {
            b3World_SetWorkerCount(m_Impl->WorldId, static_cast<int>(workerCount));
            m_Impl->AppliedWorkerCount = workerCount;
            HE_CORE_INFO(
                "Physics worker count is now {}{}",
                workerCount,
                m_Impl->Scheduler->UsesThreadPool() ? " on the job system" : " (single threaded)");
        }

        m_Impl->ApplyWorldSettings();

        // Create new bodies, update shared settings, and destroy bodies whose
        // entities were removed from the registry since the last update.
        const bool sensorEvents = m_Impl->HasTriggers(scene);
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
                m_Impl->UnregisterBody(entityKey);
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
                    scene.GetWorldTransform(entity),
                    m_Impl->ContactEventsEnabled,
                    sensorEvents);
                if (B3_IS_NON_NULL(bodyId))
                {
                    m_Impl->RegisterBody(entityKey, bodyId);
                }
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

        // Sweep bodies whose entity left the registry, or whose key the registry recycled for a
        // different entity. Collecting the keys first keeps the map from being walked while it is
        // being erased.
        std::vector<uint64_t> staleKeys;
        staleKeys.reserve(m_Impl->Bodies.size());
        for (const auto& [entityKey, bodyId] : m_Impl->Bodies)
        {
            (void)bodyId;
            if (!currentEntityKeys.contains(entityKey))
            {
                staleKeys.push_back(entityKey);
            }
        }

        for (const uint64_t entityKey : staleKeys)
        {
            m_Impl->UnregisterBody(entityKey);
        }

        // Box3D keeps the event flags on the shape, so a runtime toggle has to be applied to the
        // shapes that already exist. Done after the sweep, so a body destroyed above is not walked,
        // and before the step, so this step already reports - or already stays quiet - as asked.
        m_Impl->RefreshEventSettings(m_Impl->Settings.EnableContactEvents, sensorEvents);

        // One step per call. The accumulator that decides how many steps a frame owes lives in
        // Scene, so a script's FixedUpdate and a physics step are the same event rather than two
        // loops that can drift apart.
        //
        // Remember where every dynamic body is before the step: rendering interpolates between this
        // and the simulated result, which is what removes the stutter of drawing a 60 Hz simulation
        // on a faster display.
        for (const entt::entity entity : view)
        {
            auto& rigidbody = view.get<RigidbodyComponent>(entity);
            if (rigidbody.Type != RigidbodyComponent::RigidbodyType::Dynamic)
            {
                continue;
            }

            const auto& transform = view.get<TransformComponent>(entity);
            rigidbody.PreviousPosition = transform.Position;
            rigidbody.PreviousRotation = transform.Rotation;
            rigidbody.PreviousTransformValid = true;
        }

        b3World_Step(m_Impl->WorldId, timestep.GetSeconds(), std::max(m_Impl->Settings.SubStepCount, 1));

        // Read events before anything else can step the world: the arrays describe this step and are
        // overwritten by the next one.
        m_Impl->CollectEvents();

        // Write simulated world transforms back into the ECS.
        //
        // Driven by the step's body move events rather than by asking every dynamic body for its
        // transform: the events are published as one contiguous array, only for bodies that actually
        // moved, and they carry the entity key. A body that did not move does not need its ECS
        // transform touched, which is most of them once a scene settles.
        //
        // Scale stays editor-authored because rigid bodies do not change scale.
        const b3BodyEvents bodyEvents = b3World_GetBodyEvents(m_Impl->WorldId);
        for (int index = 0; index < bodyEvents.moveCount; ++index)
        {
            const b3BodyMoveEvent& moveEvent = bodyEvents.moveEvents[index];
            const uint64_t entityKey = DecodePhysicsBodyKey(moveEvent.userData);
            if (moveEvent.userData == nullptr)
            {
                continue;
            }

            const entt::entity handle = static_cast<entt::entity>(entityKey);
            if (!scene.GetRegistry().valid(handle))
            {
                continue;
            }

            // Read from the registry, not through a view captured before the step: a body whose entity
            // lost one of these components during this update is still possible and must not be
            // written to.
            auto& transform = scene.GetRegistry().get<TransformComponent>(handle);

            Math::Mat4 worldTransform = ToMath(moveEvent.transform);
            if (const auto* relationship = scene.GetRegistry().try_get<RelationshipComponent>(handle);
                relationship != nullptr && relationship->Parent != UUID::Invalid())
            {
                const Entity parent = scene.GetEntityByUUID(relationship->Parent);
                if (parent)
                {
                    const Math::Mat4 parentWorld = scene.GetWorldTransform(parent.GetHandle());
                    worldTransform = Math::Inverse(parentWorld) * worldTransform;
                }
            }

            transform.Position = Math::DecomposePosition(worldTransform);
            transform.Rotation = Math::Degrees(DecomposeEulerRadians(worldTransform));
        }
    }

    void PhysicsWorld::DestroyBody(uint64_t entityKey)
    {
        if (m_Impl == nullptr)
        {
            return;
        }

        m_Impl->UnregisterBody(entityKey);
    }

    bool PhysicsWorld::HasBody(uint64_t entityKey) const
    {
        return m_Impl != nullptr && m_Impl->Bodies.contains(entityKey);
    }

    bool PhysicsWorld::IsRunning() const
    {
        return m_Impl != nullptr && B3_IS_NON_NULL(m_Impl->WorldId);
    }

    const PhysicsEventQueue& PhysicsWorld::GetEvents() const
    {
        static const PhysicsEventQueue emptyQueue;
        return m_Impl != nullptr ? m_Impl->Events : emptyQueue;
    }

    PhysicsTaskStats PhysicsWorld::GetTaskStats() const
    {
        return m_Impl != nullptr ? m_Impl->Scheduler->GetStats() : PhysicsTaskStats {};
    }

    void PhysicsWorld::DrawDebug(PhysicsDebugSink& sink, const PhysicsDebugDrawOptions& options) const
    {
        if (m_Impl == nullptr || B3_IS_NULL(m_Impl->WorldId))
        {
            return;
        }

        // Box3D's default fills every callback with a no-op, so an option this adapter does not
        // implement calls a function that does nothing instead of a null pointer. Only the callbacks
        // with something to say are overridden.
        //
        // Which ones those are follows the backend rather than this engine: b3World_Draw draws shape
        // geometry through DrawShapeFcn alone and never calls the sphere, capsule or box callbacks -
        // those belong to its recording player and to joint frames - so colliders come from the shape
        // records above, and the rest of the set is here because it is one contract.
        b3DebugDraw draw = b3DefaultDebugDraw();
        draw.DrawShapeFcn = &DrawDebugShape;
        draw.DrawSegmentFcn = &DrawDebugSegment;
        draw.DrawPointFcn = &DrawDebugPoint;
        draw.DrawSphereFcn = &DrawDebugSphere;
        draw.DrawCapsuleFcn = &DrawDebugCapsule;
        draw.DrawBoundsFcn = &DrawDebugBounds;
        draw.DrawBoxFcn = &DrawDebugBox;
        // DrawTransformFcn and DrawStringFcn keep their empty implementations: this sink has no text,
        // and the mass axis and body name labels are diagnostics the editor does not show.
        draw.context = &sink;

        draw.drawShapes = options.Shapes;
        draw.drawBounds = options.Bounds;
        draw.drawContacts = options.Contacts;
        // Box3D draws contact features, forces and graph colours through the same callbacks, and a
        // line renderer cannot show the difference between them, so they stay off rather than
        // producing a mess of overlapping marks.
        draw.drawJoints = false;
        draw.drawJointExtras = false;
        draw.drawMass = false;
        draw.drawBodyNames = false;
        draw.drawGraphColors = false;
        draw.drawContactFeatures = false;
        draw.drawContactNormals = options.Contacts;
        draw.drawContactForces = false;
        draw.drawFrictionForces = false;
        draw.drawIslands = false;

        // Only what overlaps this box is drawn, so it has to cover the scene the caller is looking at.
        const float extent = std::max(options.Extent, 0.001f);
        draw.drawingBounds = {
            { -extent, -extent, -extent },
            { extent, extent, extent }
        };

        b3World_Draw(m_Impl->WorldId, &draw, B3_DEFAULT_CATEGORY_BITS);
    }

    // ---------------------------------------------------------------------- queries

    namespace
    {
        // Zero, or a direction too short to define a ray. Reported as "no hit" rather than as a
        // degenerate query, because the callers that can produce it - a script aiming a ray from a
        // UI value - would otherwise get an arbitrary shape back.
        bool IsDegenerate(const Math::Vec3& direction, float maxDistance)
        {
            return maxDistance <= 0.0f || Math::Length(direction) <= 1e-5f;
        }
    }

    bool PhysicsWorld::RayCast(
        const Math::Vec3& origin,
        const Math::Vec3& direction,
        float maxDistance,
        RayCastResult& outResult,
        const PhysicsQueryFilter& filter) const
    {
        if (m_Impl == nullptr || B3_IS_NULL(m_Impl->WorldId) || IsDegenerate(direction, maxDistance))
        {
            return false;
        }

        b3QueryFilter queryFilter;
        if (!ToBox3D(filter, queryFilter))
        {
            return false;
        }

        const Math::Vec3 unitDirection = Math::Normalize(direction);

        CastHit hit;
        hit.Filter = filter;
        b3World_CastRay(
            m_Impl->WorldId,
            ToBox3D(origin),
            ToBox3D(unitDirection * maxDistance),
            queryFilter,
            &RayCastCallback,
            &hit);

        if (!hit.Valid)
        {
            return false;
        }

        outResult.EntityKey = hit.EntityKey;
        outResult.Point = hit.Point;
        outResult.Normal = hit.Normal;
        outResult.Distance = hit.Fraction * maxDistance;
        return true;
    }

    size_t PhysicsWorld::RayCastAll(
        const Math::Vec3& origin,
        const Math::Vec3& direction,
        float maxDistance,
        RayCastResults& outResults,
        size_t maxResults,
        const PhysicsQueryFilter& filter) const
    {
        if (m_Impl == nullptr || B3_IS_NULL(m_Impl->WorldId) || IsDegenerate(direction, maxDistance)
            || maxResults == 0)
        {
            return 0;
        }

        b3QueryFilter queryFilter;
        if (!ToBox3D(filter, queryFilter))
        {
            return 0;
        }

        const Math::Vec3 unitDirection = Math::Normalize(direction);
        const b3Vec3 translation = ToBox3D(unitDirection * maxDistance);

        RayCollectContext collector;
        collector.Filter = filter;
        collector.Translation = translation;
        RayCastResults hits;
        collector.Results = &hits;
        collector.MaxResults = maxResults;
        b3World_CastRay(m_Impl->WorldId, ToBox3D(origin), translation, queryFilter, &RayCollectCallback, &collector);
        std::sort(hits.begin(), hits.end(), [](const auto& a, const auto& b) { return a.Distance < b.Distance; });
        outResults.insert(outResults.end(), hits.begin(), hits.end());
        return hits.size();
    }

    size_t PhysicsWorld::OverlapSphere(
        const Math::Vec3& center,
        float radius,
        OverlapResults& outResults,
        size_t maxResults,
        const PhysicsQueryFilter& filter) const
    {
        if (m_Impl == nullptr || B3_IS_NULL(m_Impl->WorldId) || radius <= 0.0f || maxResults == 0)
        {
            return 0;
        }

        b3QueryFilter queryFilter;
        if (!ToBox3D(filter, queryFilter))
        {
            return 0;
        }

        // A sphere proxy is one point with the sphere's radius, measured from the query origin.
        const b3Vec3 point { 0.0f, 0.0f, 0.0f };
        b3ShapeProxy proxy;
        proxy.points = &point;
        proxy.count = 1;
        proxy.radius = radius;

        OverlapCollectContext collector;
        collector.Filter = filter;
        collector.Results = &outResults;
        collector.MaxResults = outResults.size() + std::min(maxResults, outResults.max_size() - outResults.size());

        const size_t addedBefore = outResults.size();
        b3World_OverlapShape(m_Impl->WorldId, ToBox3D(center), &proxy, queryFilter, &OverlapCallback, &collector);

        return outResults.size() - addedBefore;
    }

    size_t PhysicsWorld::OverlapBox(
        const Math::Vec3& center,
        const Math::Vec3& halfExtents,
        const Math::Quat& rotation,
        OverlapResults& outResults,
        size_t maxResults,
        const PhysicsQueryFilter& filter) const
    {
        if (m_Impl == nullptr || B3_IS_NULL(m_Impl->WorldId) || maxResults == 0)
        {
            return 0;
        }

        b3QueryFilter queryFilter;
        if (!ToBox3D(filter, queryFilter))
        {
            return 0;
        }

        // A box proxy is its eight corners with no radius. Rotating them here keeps the overlap test
        // exact, where an axis-aligned box around the rotated one would report shapes the box never
        // reaches.
        b3Vec3 corners[8] {};
        FillBoxProxyPoints(halfExtents, corners);

        const Math::Mat3 rotationMatrix(rotation);
        for (b3Vec3& corner : corners)
        {
            corner = ToBox3D(rotationMatrix * ToMath(corner));
        }

        b3ShapeProxy proxy;
        proxy.points = corners;
        proxy.count = 8;
        proxy.radius = 0.0f;

        OverlapCollectContext collector;
        collector.Filter = filter;
        collector.Results = &outResults;
        collector.MaxResults = outResults.size() + std::min(maxResults, outResults.max_size() - outResults.size());

        const size_t addedBefore = outResults.size();
        b3World_OverlapShape(m_Impl->WorldId, ToBox3D(center), &proxy, queryFilter, &OverlapCallback, &collector);

        return outResults.size() - addedBefore;
    }

    bool PhysicsWorld::ShapeCastSphere(
        const Math::Vec3& origin,
        const Math::Vec3& translation,
        float radius,
        ShapeCastResult& outResult,
        const PhysicsQueryFilter& filter) const
    {
        const float distance = Math::Length(translation);
        if (m_Impl == nullptr || B3_IS_NULL(m_Impl->WorldId) || IsDegenerate(translation, distance)
            || radius <= 0.0f)
        {
            return false;
        }

        b3QueryFilter queryFilter;
        if (!ToBox3D(filter, queryFilter))
        {
            return false;
        }

        const b3Vec3 point { 0.0f, 0.0f, 0.0f };
        b3ShapeProxy proxy;
        proxy.points = &point;
        proxy.count = 1;
        proxy.radius = radius;

        CastHit hit;
        hit.Filter = filter;
        b3World_CastShape(
            m_Impl->WorldId,
            ToBox3D(origin),
            &proxy,
            ToBox3D(translation),
            queryFilter,
            &ShapeCastCallback,
            &hit);

        if (!hit.Valid)
        {
            return false;
        }

        outResult.EntityKey = hit.EntityKey;
        outResult.Point = hit.Point;
        outResult.Normal = hit.Normal;
        // A sphere that starts already overlapping reports a fraction of 0: it did not travel, and
        // saying so is what lets a character controller decide whether it is stuck.
        outResult.Distance = hit.Fraction * distance;
        return true;
    }

    // ----------------------------------------------------------------- motion state
    Math::Vec3 PhysicsWorld::GetLinearVelocity(uint64_t entityKey) const
    {
        b3BodyId bodyId;
        return m_Impl != nullptr && m_Impl->TryFindBody(entityKey, bodyId)
            ? ToMath(b3Body_GetLinearVelocity(bodyId))
            : Math::Vec3(0.0f);
    }

    void PhysicsWorld::SetLinearVelocity(uint64_t entityKey, const Math::Vec3& velocity)
    {
        b3BodyId bodyId;
        if (m_Impl != nullptr && m_Impl->TryFindBody(entityKey, bodyId))
        {
            b3Body_SetLinearVelocity(bodyId, ToBox3D(velocity));
        }
    }

    Math::Vec3 PhysicsWorld::GetAngularVelocity(uint64_t entityKey) const
    {
        b3BodyId bodyId;
        return m_Impl != nullptr && m_Impl->TryFindBody(entityKey, bodyId)
            ? ToMath(b3Body_GetAngularVelocity(bodyId))
            : Math::Vec3(0.0f);
    }

    void PhysicsWorld::SetAngularVelocity(uint64_t entityKey, const Math::Vec3& velocity)
    {
        b3BodyId bodyId;
        if (m_Impl != nullptr && m_Impl->TryFindBody(entityKey, bodyId))
        {
            b3Body_SetAngularVelocity(bodyId, ToBox3D(velocity));
        }
    }

    void PhysicsWorld::ApplyForce(uint64_t entityKey, const Math::Vec3& force)
    {
        b3BodyId bodyId;
        if (m_Impl != nullptr && m_Impl->TryFindBody(entityKey, bodyId))
        {
            b3Body_ApplyForceToCenter(bodyId, ToBox3D(force), true);
        }
    }

    void PhysicsWorld::ApplyForceAtPoint(uint64_t entityKey, const Math::Vec3& force, const Math::Vec3& worldPoint)
    {
        b3BodyId bodyId;
        if (m_Impl != nullptr && m_Impl->TryFindBody(entityKey, bodyId))
        {
            b3Body_ApplyForce(bodyId, ToBox3D(force), ToBox3D(worldPoint), true);
        }
    }

    void PhysicsWorld::ApplyImpulse(uint64_t entityKey, const Math::Vec3& impulse)
    {
        b3BodyId bodyId;
        if (m_Impl != nullptr && m_Impl->TryFindBody(entityKey, bodyId))
        {
            b3Body_ApplyLinearImpulseToCenter(bodyId, ToBox3D(impulse), true);
        }
    }

    void PhysicsWorld::ApplyImpulseAtPoint(uint64_t entityKey, const Math::Vec3& impulse, const Math::Vec3& worldPoint)
    {
        b3BodyId bodyId;
        if (m_Impl != nullptr && m_Impl->TryFindBody(entityKey, bodyId))
        {
            b3Body_ApplyLinearImpulse(bodyId, ToBox3D(impulse), ToBox3D(worldPoint), true);
        }
    }

    void PhysicsWorld::ApplyTorque(uint64_t entityKey, const Math::Vec3& torque)
    {
        b3BodyId bodyId;
        if (m_Impl != nullptr && m_Impl->TryFindBody(entityKey, bodyId))
        {
            b3Body_ApplyTorque(bodyId, ToBox3D(torque), true);
        }
    }

    void PhysicsWorld::ApplyAngularImpulse(uint64_t entityKey, const Math::Vec3& impulse)
    {
        b3BodyId bodyId;
        if (m_Impl != nullptr && m_Impl->TryFindBody(entityKey, bodyId))
        {
            b3Body_ApplyAngularImpulse(bodyId, ToBox3D(impulse), true);
        }
    }

    float PhysicsWorld::GetMass(uint64_t entityKey) const
    {
        b3BodyId bodyId;
        return m_Impl != nullptr && m_Impl->TryFindBody(entityKey, bodyId) ? b3Body_GetMass(bodyId) : 0.0f;
    }

    bool PhysicsWorld::IsAwake(uint64_t entityKey) const
    {
        b3BodyId bodyId;
        return m_Impl != nullptr && m_Impl->TryFindBody(entityKey, bodyId) && b3Body_IsAwake(bodyId);
    }

    void PhysicsWorld::SetAwake(uint64_t entityKey, bool awake)
    {
        b3BodyId bodyId;
        if (m_Impl != nullptr && m_Impl->TryFindBody(entityKey, bodyId))
        {
            b3Body_SetAwake(bodyId, awake);
        }
    }

    void PhysicsWorld::Teleport(
        uint64_t entityKey,
        const Math::Vec3& position,
        const Math::Quat& rotation,
        Scene& scene)
    {
        if (m_Impl == nullptr)
        {
            return;
        }

        const entt::entity handle = static_cast<entt::entity>(entityKey);
        if (!scene.GetRegistry().valid(handle)
            || !scene.GetRegistry().all_of<TransformComponent>(handle))
        {
            return;
        }

        // The requested transform is world space, and the entity's own transform is local, so the
        // parent chain is divided out - the inverse of what the write-back does when it composes the
        // body's world transform back into the entity's space.
        const Math::Mat4 worldTransform = Math::Translate(Math::Mat4(1.0f), position) * Math::ToMat4(rotation);
        Math::Mat4 localTransform = worldTransform;

        const auto* relationship = scene.GetRegistry().try_get<RelationshipComponent>(handle);
        if (relationship != nullptr && relationship->Parent != UUID::Invalid())
        {
            const Entity parent = scene.GetEntityByUUID(relationship->Parent);
            if (parent)
            {
                const Math::Mat4 parentWorld = scene.GetWorldTransform(parent.GetHandle());
                localTransform = Math::Inverse(parentWorld) * localTransform;
            }
        }

        const Math::Vec3 localPosition = Math::DecomposePosition(localTransform);
        const Math::Vec3 localRotation = Math::Degrees(DecomposeEulerRadians(localTransform));

        // The scene transform is written even when the entity has no body: the caller asked for the
        // entity to be somewhere, and leaving it behind would be a silent half-teleport.
        auto& transform = scene.GetRegistry().get<TransformComponent>(handle);
        transform.Position = localPosition;
        transform.Rotation = localRotation;

        if (auto* rigidbody = scene.GetRegistry().try_get<RigidbodyComponent>(handle))
        {
            // The move was not simulated, so there is nothing to interpolate towards: without this
            // the entity would be drawn sliding from where it used to be over the next frame.
            rigidbody->PreviousPosition = localPosition;
            rigidbody->PreviousRotation = localRotation;
        }

        const auto bodyIt = m_Impl->Bodies.find(entityKey);
        if (bodyIt == m_Impl->Bodies.end())
        {
            return;
        }

        const b3BodyId bodyId = bodyIt->second;
        b3Vec3 bodyPosition;
        b3Quat bodyRotation;
        Math::Vec3 effectiveScale;
        DecomposeWorldTransform(worldTransform, bodyPosition, bodyRotation, effectiveScale);

        // Velocities are deliberately left alone: a teleport says where the body is, not how fast it
        // is going, and a character controller that keeps its speed through a teleport depends on
        // that. The body is woken so the new position is reflected in the next step.
        b3Body_SetTransform(bodyId, bodyPosition, bodyRotation);
        b3Body_SetAwake(bodyId, true);
    }
}
