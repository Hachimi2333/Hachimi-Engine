#include "Panels/InspectorPanel.h"

#include "Asset/AssetManager.h"
#include "Panels/EditorContext.h"
#include "Renderer/MeshFactory.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Utils/FileDialogs.h"
#include "Math/Math.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace HachimiEngine
{
    namespace
    {
        // Draws a collapsible component header with a right-aligned remove button.
        template<typename T>
        bool DrawComponentHeader(Entity entity, const char* label, bool defaultOpen, bool& removed)
        {
            const ImGuiTreeNodeFlags flags = defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;
            const bool open = ImGui::CollapsingHeader(label, flags);

            const float buttonWidth = ImGui::GetFrameHeight();
            ImGui::SameLine(std::max(ImGui::GetContentRegionAvail().x - buttonWidth, 0.0f));

            ImGui::PushID(label);
            const bool removeClicked = ImGui::SmallButton("x");
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Remove component");
            }
            ImGui::PopID();

            if (removeClicked)
            {
                entity.RemoveComponent<T>();
                removed = true;
            }

            return open;
        }

        // Creates sensible collider dimensions for the built-in mesh primitives.
        void ConfigureColliderForPrimitive(ColliderComponent& collider, PrimitiveMeshType primitiveType)
        {
            switch (primitiveType)
            {
                case PrimitiveMeshType::Sphere:
                    collider.ShapeType = ColliderComponent::ColliderShapeType::Sphere;
                    collider.Radius = 0.5f;
                    break;
                case PrimitiveMeshType::Plane:
                    collider.ShapeType = ColliderComponent::ColliderShapeType::Plane;
                    collider.HalfExtents = { 5.0f, 0.05f, 5.0f };
                    break;
                case PrimitiveMeshType::Cube:
                case PrimitiveMeshType::Grid:
                case PrimitiveMeshType::None:
                default:
                    collider.ShapeType = ColliderComponent::ColliderShapeType::Box;
                    collider.HalfExtents = { 0.5f, 0.5f, 0.5f };
                    break;
            }
        }

        void AddDefaultCollider(Entity entity)
        {
            auto& collider = entity.AddComponent<ColliderComponent>();
            if (entity.HasComponent<MeshComponent>())
            {
                ConfigureColliderForPrimitive(collider, entity.GetComponent<MeshComponent>().PrimitiveType);
            }
        }

        void MakeScriptPathRelative(const std::string& selectedPath, ScriptComponent::ScriptReference& reference)
        {
            const std::filesystem::path scriptsDirectory = AssetManager::GetAssetsDirectory() / "Scripts";

            std::error_code errorCode;
            const std::filesystem::path relativePath = std::filesystem::relative(selectedPath, scriptsDirectory, errorCode);
            if (errorCode)
            {
                reference.Path = std::filesystem::path(selectedPath).filename().string();
                return;
            }

            reference.Path = relativePath.generic_string();
        }
    }

    void InspectorPanel::Draw(EditorContext& context)
    {
        ImGui::Begin("Inspector");

        if (!context.SelectedEntity || context.ActiveScene == nullptr)
        {
            ImGui::TextDisabled("No entity selected");
            ImGui::End();
            return;
        }

        Entity entity = context.SelectedEntity;
        char tagBuffer[128] = {};
        std::snprintf(tagBuffer, sizeof(tagBuffer), "%s", entity.GetName().c_str());

        if (ImGui::InputText("Tag", tagBuffer, sizeof(tagBuffer)))
        {
            entity.GetComponent<TagComponent>().Tag = tagBuffer;
        }

        ImGui::Text("UUID: %s", entity.GetUUID().ToString().c_str());
        ImGui::Separator();

        DrawTransform(entity);

        if (entity.HasComponent<RigidbodyComponent>())
        {
            DrawRigidbody(entity);
        }
        if (entity.HasComponent<ColliderComponent>())
        {
            DrawCollider(entity);
        }
        if (entity.HasComponent<MeshComponent>())
        {
            DrawMesh(entity);
        }
        if (entity.HasComponent<CameraComponent>())
        {
            DrawCamera(entity);
        }
        if (entity.HasComponent<LightComponent>())
        {
            DrawLight(entity);
        }
        if (entity.HasComponent<ScriptComponent>())
        {
            DrawScript(entity);
        }

        ImGui::Separator();
        DrawAddComponentMenu(context, entity);

        if (ImGui::Button("Delete Entity"))
        {
            context.ActiveScene->DestroyEntity(entity);
            context.SelectedEntity = {};
        }

        ImGui::End();
    }

    void InspectorPanel::DrawAddComponentMenu(EditorContext& context, Entity entity)
    {
        if (ImGui::Button("Add Component"))
        {
            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            if (!entity.HasComponent<TransformComponent>() && ImGui::MenuItem("Transform Component"))
            {
                entity.AddComponent<TransformComponent>();
            }
            if (!entity.HasComponent<MeshComponent>() && ImGui::MenuItem("Mesh Component"))
            {
                auto& mesh = entity.AddComponent<MeshComponent>();
                mesh.PrimitiveType = PrimitiveMeshType::Cube;
                mesh.Mesh = MeshFactory::CreateCube();
            }
            if (!entity.HasComponent<RigidbodyComponent>() && ImGui::MenuItem("Rigidbody Component"))
            {
                entity.AddComponent<RigidbodyComponent>();
                if (!entity.HasComponent<ColliderComponent>())
                {
                    AddDefaultCollider(entity);
                }
            }
            if (!entity.HasComponent<ColliderComponent>() && ImGui::MenuItem("Box Collider"))
            {
                auto& collider = entity.AddComponent<ColliderComponent>();
                collider.ShapeType = ColliderComponent::ColliderShapeType::Box;
            }
            if (!entity.HasComponent<ColliderComponent>() && ImGui::MenuItem("Sphere Collider"))
            {
                auto& collider = entity.AddComponent<ColliderComponent>();
                collider.ShapeType = ColliderComponent::ColliderShapeType::Sphere;
            }
            if (!entity.HasComponent<ColliderComponent>() && ImGui::MenuItem("Capsule Collider"))
            {
                auto& collider = entity.AddComponent<ColliderComponent>();
                collider.ShapeType = ColliderComponent::ColliderShapeType::Capsule;
                collider.Radius = 0.25f;
                collider.Height = 1.0f;
            }
            if (!entity.HasComponent<ColliderComponent>() && ImGui::MenuItem("Plane Collider"))
            {
                auto& collider = entity.AddComponent<ColliderComponent>();
                collider.ShapeType = ColliderComponent::ColliderShapeType::Plane;
                collider.HalfExtents = { 5.0f, 0.05f, 5.0f };
            }
            if (!entity.HasComponent<CameraComponent>() && ImGui::MenuItem("Camera Component"))
            {
                entity.AddComponent<CameraComponent>();
            }
            if (!entity.HasComponent<LightComponent>() && ImGui::MenuItem("Light Component"))
            {
                entity.AddComponent<LightComponent>();
            }
            if (!entity.HasComponent<ScriptComponent>() && ImGui::MenuItem("Script Component"))
            {
                entity.AddComponent<ScriptComponent>().Scripts.emplace_back();
            }
            ImGui::EndPopup();
        }
    }

    void InspectorPanel::DrawTransform(Entity entity)
    {
        if (!entity.HasComponent<TransformComponent>())
        {
            return;
        }

        bool removed = false;
        const bool open = DrawComponentHeader<TransformComponent>(entity, "Transform", true, removed);
        if (removed || !open)
        {
            return;
        }

        auto& transform = entity.Transform();
        ImGui::DragFloat3("Position", Math::ValuePtr(transform.Position), 0.05f);
        ImGui::DragFloat3("Rotation", Math::ValuePtr(transform.Rotation), 0.25f);
        ImGui::DragFloat3("Scale", Math::ValuePtr(transform.Scale), 0.05f, 0.01f, 100.0f);
    }

    void InspectorPanel::DrawRigidbody(Entity entity)
    {
        bool removed = false;
        const bool open = DrawComponentHeader<RigidbodyComponent>(entity, "Rigidbody", true, removed);
        if (removed || !open)
        {
            return;
        }

        auto& rigidbody = entity.GetComponent<RigidbodyComponent>();

        const char* typeNames[] = { "Static", "Kinematic", "Dynamic" };
        int type = static_cast<int>(rigidbody.Type);
        if (ImGui::Combo("Type", &type, typeNames, IM_ARRAYSIZE(typeNames)))
        {
            rigidbody.Type = static_cast<RigidbodyComponent::RigidbodyType>(type);
        }

        if (rigidbody.Type != RigidbodyComponent::RigidbodyType::Static)
        {
            ImGui::DragFloat3("Linear Velocity", Math::ValuePtr(rigidbody.LinearVelocity), 0.05f);
            ImGui::DragFloat3("Angular Velocity", Math::ValuePtr(rigidbody.AngularVelocity), 0.05f);
        }

        ImGui::DragFloat("Linear Damping", &rigidbody.LinearDamping, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Angular Damping", &rigidbody.AngularDamping, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Gravity Scale", &rigidbody.GravityScale, 0.05f, 0.0f, 10.0f);
        ImGui::Checkbox("Enable Sleep", &rigidbody.EnableSleep);
        ImGui::Checkbox("Initially Awake", &rigidbody.InitiallyAwake);
        ImGui::Checkbox("Is Bullet", &rigidbody.IsBullet);
        ImGui::Checkbox("Enabled", &rigidbody.IsEnabled);
    }

    void InspectorPanel::DrawCollider(Entity entity)
    {
        bool removed = false;
        const bool open = DrawComponentHeader<ColliderComponent>(entity, "Collider", true, removed);
        if (removed || !open)
        {
            return;
        }

        auto& collider = entity.GetComponent<ColliderComponent>();

        const char* shapeNames[] = { "Box", "Sphere", "Capsule", "Plane" };
        int shapeType = static_cast<int>(collider.ShapeType);
        if (ImGui::Combo("Shape", &shapeType, shapeNames, IM_ARRAYSIZE(shapeNames)))
        {
            collider.ShapeType = static_cast<ColliderComponent::ColliderShapeType>(shapeType);
        }

        switch (collider.ShapeType)
        {
            case ColliderComponent::ColliderShapeType::Box:
                ImGui::DragFloat3("Half Extents", Math::ValuePtr(collider.HalfExtents), 0.05f, 0.01f, 100.0f);
                break;
            case ColliderComponent::ColliderShapeType::Sphere:
                ImGui::DragFloat("Radius", &collider.Radius, 0.05f, 0.01f, 100.0f);
                break;
            case ColliderComponent::ColliderShapeType::Capsule:
                ImGui::DragFloat("Radius", &collider.Radius, 0.05f, 0.01f, 100.0f);
                ImGui::DragFloat("Height", &collider.Height, 0.05f, 0.01f, 100.0f);
                break;
            case ColliderComponent::ColliderShapeType::Plane:
                ImGui::DragFloat("Half Width", &collider.HalfExtents.x, 0.05f, 0.01f, 1000.0f);
                ImGui::DragFloat("Half Depth", &collider.HalfExtents.z, 0.05f, 0.01f, 1000.0f);
                break;
        }

        ImGui::DragFloat3("Offset", Math::ValuePtr(collider.Offset), 0.05f);
        ImGui::DragFloat("Density", &collider.Density, 0.05f, 0.0f, 100000.0f);
        ImGui::SliderFloat("Friction", &collider.Friction, 0.0f, 1.0f);
        ImGui::SliderFloat("Restitution", &collider.Restitution, 0.0f, 1.0f);
        ImGui::SliderFloat("Rolling Resistance", &collider.RollingResistance, 0.0f, 1.0f);
        ImGui::Checkbox("Is Trigger", &collider.IsTrigger);
        ImGui::InputScalar("Category Bits", ImGuiDataType_U64, &collider.CategoryBits, nullptr, nullptr, "%llX", ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::InputScalar("Mask Bits", ImGuiDataType_U64, &collider.MaskBits, nullptr, nullptr, "%llX", ImGuiInputTextFlags_CharsHexadecimal);
    }

    void InspectorPanel::DrawMesh(Entity entity)
    {
        bool removed = false;
        const bool open = DrawComponentHeader<MeshComponent>(entity, "Mesh", true, removed);
        if (removed || !open)
        {
            return;
        }

        auto& mesh = entity.GetComponent<MeshComponent>();

        const char* primitiveNames[] = { "Cube", "Sphere", "Plane", "Grid" };
        int primitiveType = static_cast<int>(mesh.PrimitiveType) - static_cast<int>(PrimitiveMeshType::Cube);
        if (primitiveType < 0)
        {
            primitiveType = 0;
        }

        if (ImGui::Combo("Primitive", &primitiveType, primitiveNames, IM_ARRAYSIZE(primitiveNames)))
        {
            mesh.PrimitiveType = static_cast<PrimitiveMeshType>(primitiveType + static_cast<int>(PrimitiveMeshType::Cube));
            mesh.Mesh = MeshFactory::CreatePrimitive(mesh.PrimitiveType);
        }

        ImGui::ColorEdit4("Albedo Color", Math::ValuePtr(mesh.MaterialColor));
        ImGui::SliderFloat("Roughness", &mesh.Roughness, 0.01f, 1.0f);
        ImGui::SliderFloat("Metallic", &mesh.Metallic, 0.0f, 1.0f);
        ImGui::Checkbox("Visible", &mesh.Visible);

        if (mesh.MaterialOverride != nullptr)
        {
            mesh.MaterialOverride->SetAlbedoColor(mesh.MaterialColor);
            mesh.MaterialOverride->SetRoughness(mesh.Roughness);
            mesh.MaterialOverride->SetMetallic(mesh.Metallic);
        }
    }

    void InspectorPanel::DrawCamera(Entity entity)
    {
        bool removed = false;
        const bool open = DrawComponentHeader<CameraComponent>(entity, "Camera", true, removed);
        if (removed || !open)
        {
            return;
        }

        auto& camera = entity.GetComponent<CameraComponent>();

        ImGui::Checkbox("Primary", &camera.Primary);
        ImGui::SliderFloat("Field Of View", &camera.FieldOfView, 20.0f, 120.0f);
        ImGui::DragFloat("Near Clip", &camera.NearClip, 0.01f, 0.001f, 10.0f);
        ImGui::DragFloat("Far Clip", &camera.FarClip, 1.0f, 10.0f, 10000.0f);
    }

    void InspectorPanel::DrawLight(Entity entity)
    {
        bool removed = false;
        const bool open = DrawComponentHeader<LightComponent>(entity, "Light", true, removed);
        if (removed || !open)
        {
            return;
        }

        auto& light = entity.GetComponent<LightComponent>();

        const char* lightTypeNames[] = { "Directional", "Point" };
        int lightType = static_cast<int>(light.Type);
        if (ImGui::Combo("Type", &lightType, lightTypeNames, IM_ARRAYSIZE(lightTypeNames)))
        {
            light.Type = static_cast<LightComponent::LightType>(lightType);
        }

        ImGui::ColorEdit3("Color", Math::ValuePtr(light.Color));
        ImGui::DragFloat("Intensity", &light.Intensity, 0.1f, 0.0f, 1000.0f);
        if (light.Type == LightComponent::LightType::Point)
        {
            ImGui::DragFloat("Range", &light.Range, 0.1f, 0.1f, 1000.0f);
        }
        else
        {
            ImGui::Checkbox("Casts Shadows", &light.CastsShadows);
            ImGui::DragFloat("Shadow Bias", &light.ShadowBias, 0.0001f, 0.0f, 0.05f, "%.5f");
        }
    }

    void InspectorPanel::DrawScript(Entity entity)
    {
        bool removed = false;
        const bool open = DrawComponentHeader<ScriptComponent>(entity, "Script", true, removed);
        if (removed || !open)
        {
            return;
        }

        auto& script = entity.GetComponent<ScriptComponent>();

        int removeSlot = -1;
        for (int slotIndex = 0; slotIndex < static_cast<int>(script.Scripts.size()); ++slotIndex)
        {
            ScriptComponent::ScriptReference& reference = script.Scripts[slotIndex];

            ImGui::PushID(slotIndex);
            ImGui::Checkbox("Enabled", &reference.Enabled);

            char pathBuffer[256] = {};
            std::snprintf(pathBuffer, sizeof(pathBuffer), "%s", reference.Path.c_str());
            ImGui::SetNextItemWidth(std::max(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x, 40.0f));
            if (ImGui::InputText("Path", pathBuffer, sizeof(pathBuffer)))
            {
                reference.Path = pathBuffer;
            }

            ImGui::SameLine();
            if (ImGui::Button("..."))
            {
                m_PendingScriptFileDialogSlot = slotIndex;
                FileDialogs::OpenScriptFileDialog(AssetManager::GetAssetsDirectory() / "Scripts");
            }

            if (ImGui::SmallButton("Remove Script"))
            {
                removeSlot = slotIndex;
            }
            ImGui::PopID();

            ImGui::Separator();
        }

        if (ImGui::Button("Add Script"))
        {
            script.Scripts.emplace_back();
        }

        std::string selectedPath;
        if (FileDialogs::DrawScriptFileDialog(selectedPath))
        {
            if (!selectedPath.empty() && m_PendingScriptFileDialogSlot >= 0 && m_PendingScriptFileDialogSlot < static_cast<int>(script.Scripts.size()))
            {
                MakeScriptPathRelative(selectedPath, script.Scripts[m_PendingScriptFileDialogSlot]);
            }
            m_PendingScriptFileDialogSlot = -1;
        }

        if (removeSlot >= 0)
        {
            script.Scripts.erase(script.Scripts.begin() + removeSlot);
        }
    }
}
