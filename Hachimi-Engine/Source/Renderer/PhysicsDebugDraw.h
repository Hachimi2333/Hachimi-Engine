#pragma once

#include "Core/Base.h"
#include "Math/Math.h"

#include <cstdint>

namespace HachimiEngine
{
    // Where a physics debug view is drawn to.
    //
    // The backend hands its view out as C callbacks over its own types; the adapter that receives
    // them (PhysicsWorld::DrawDebug) turns each one into these primitives, so the shapes the
    // simulation reports can be drawn by the editor, recorded by a test, or dropped by a client that
    // does not want them - without any Box3D type leaving the physics layer.
    //
    // Calls arrive in world space and in the order the backend produced them; a sink that draws
    // immediately is expected, because nothing buffers the calls.
    class PhysicsDebugSink
    {
    public:
        virtual ~PhysicsDebugSink() = default;

        virtual void Segment(const Math::Vec3& start, const Math::Vec3& end, const Math::Vec4& color) = 0;
        // Extent is a world-space arm length, already converted from the backend's point size.
        virtual void Point(const Math::Vec3& position, float extent, const Math::Vec4& color) = 0;
        virtual void Sphere(const Math::Vec3& center, float radius, const Math::Vec4& color) = 0;
        virtual void Capsule(const Math::Vec3& start, const Math::Vec3& end, float radius, const Math::Vec4& color) = 0;
        // Half extents in the box's own space, with the centre and orientation that place it.
        virtual void Box(const Math::Vec3& halfExtents, const Math::Vec3& center, const Math::Quat& rotation,
                         const Math::Vec4& color) = 0;
        virtual void Bounds(const Math::Vec3& minimum, const Math::Vec3& maximum, const Math::Vec4& color) = 0;
    };

    // What a debug draw should report. Everything is off the shelf except the shapes themselves:
    // they are the view an editor wants by default, and the rest are diagnostics that cost a pass
    // over the world each frame.
    struct PhysicsDebugDrawOptions
    {
        bool Shapes = true;
        // The fat bounding boxes the broad phase keeps, which is what explains why a pair was even
        // considered for collision.
        bool Bounds = false;
        // Contact points, normals and impulses.
        bool Contacts = false;
        // Half extent of the region the backend is asked about, centred on the origin. Box3D only
        // draws what overlaps it, so this is both a culling knob and a silent "nothing to draw" when
        // it does not cover the scene.
        float Extent = 1000.0f;
    };

    // Box3D packs a debug colour as 0xRRGGBB with a material preset in the high byte
    // (b3MakeDebugColor). Only a renderer that implements those presets can use the high byte, so the
    // low 24 bits are the colour and the alpha is ours.
    Math::Vec4 DecodePhysicsDebugColor(uint32_t packedColor, float alpha = 1.0f);

    // Box3D's point size is a pixel size for a GL_POINTS renderer - it draws contact points at 5, 10
    // and 20. This engine's debug renderer draws lines, so a point is a small cross, and this is the
    // world-space arm length that keeps the backend's proportions between the sizes it uses.
    float PhysicsDebugPointExtent(float pointSize);
}
