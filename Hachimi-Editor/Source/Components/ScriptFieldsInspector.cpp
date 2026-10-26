#include "Components/ScriptFieldsInspector.h"

#include "Components/InspectorWidgets.h"

#include <imgui.h>

#include <array>
#include <cfloat>
#include <cstdio>
#include <string>

namespace HachimiEngine
{
    namespace
    {
        constexpr std::array<const char*, 5> TypeItems { "Bool", "Int", "Float", "String", "Vec3" };

        bool HasFieldNamed(const std::vector<ScriptField>& fields, const std::string& name, size_t exceptIndex)
        {
            for (size_t index = 0; index < fields.size(); ++index)
            {
                if (index != exceptIndex && fields[index].Name == name)
                {
                    return true;
                }
            }
            return false;
        }

        // A fresh row starts on Float, which is what most script fields are, and with a name that
        // is free: two fields of the same name would share one key in the script's table.
        ScriptField MakeNewField(const std::vector<ScriptField>& fields)
        {
            ScriptField field;
            field.Name = "NewField";
            for (int suffix = 2; HasFieldNamed(fields, field.Name, fields.size()); ++suffix)
            {
                field.Name = "NewField" + std::to_string(suffix);
            }

            field.Value = ScriptFields::MakeDefaultValue(ScriptFieldType::Float);
            field.DefaultValue = field.Value;
            return field;
        }

        // Draws the widget that matches the field's type. Returns true when the value changed.
        bool DrawFieldValue(ScriptFieldData& data)
        {
            switch (ScriptFields::GetType(data))
            {
                case ScriptFieldType::Bool:
                {
                    bool value = std::get<bool>(data);
                    if (ImGui::Checkbox("##Value", &value))
                    {
                        data = value;
                        return true;
                    }
                    return false;
                }
                case ScriptFieldType::Int:
                {
                    // The field holds 64 bits so a scene file keeps what a script wrote; the widget
                    // is the 32-bit range ImGui can drag, which is what an inspector value is for.
                    int value = static_cast<int>(std::get<int64_t>(data));
                    if (ImGui::DragInt("##Value", &value, 0.1f))
                    {
                        data = static_cast<int64_t>(value);
                        return true;
                    }
                    return false;
                }
                case ScriptFieldType::Float:
                {
                    float value = std::get<float>(data);
                    if (ImGui::DragFloat("##Value", &value, 0.01f))
                    {
                        data = value;
                        return true;
                    }
                    return false;
                }
                case ScriptFieldType::String:
                {
                    char buffer[512] = {};
                    const std::string& current = std::get<std::string>(data);
                    std::snprintf(buffer, sizeof(buffer), "%s", current.c_str());
                    if (ImGui::InputText("##Value", buffer, sizeof(buffer)))
                    {
                        data = std::string(buffer);
                        return true;
                    }
                    return false;
                }
                case ScriptFieldType::Vec3:
                {
                    Math::Vec3 value = std::get<Math::Vec3>(data);
                    if (ImGui::DragFloat3("##Value", Math::ValuePtr(value), 0.01f))
                    {
                        data = value;
                        return true;
                    }
                    return false;
                }
            }

            return false;
        }
    }

    bool DrawScriptFieldsInspector(std::vector<ScriptField>& fields, bool editRuntimeValues)
    {
        bool changed = false;
        size_t removeIndex = fields.size();

        for (size_t index = 0; index < fields.size(); ++index)
        {
            ScriptField& field = fields[index];
            ImGui::PushID(static_cast<int>(index));

            if (BeginInspectorTable("InspectorScriptFieldRows"))
            {
                BeginInspectorProperty("Name");
                {
                    char nameBuffer[128] = {};
                    std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", field.Name.c_str());
                    if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer)))
                    {
                        const std::string typed = nameBuffer;
                        if (typed.empty())
                        {
                            // An empty name would be a key no script could index, so the edit is
                            // refused rather than stored.
                            ImGui::SetTooltip("A field needs a name");
                        }
                        else if (typed != field.Name)
                        {
                            if (HasFieldNamed(fields, typed, index))
                            {
                                ImGui::SetTooltip("Another field already uses this name");
                            }
                            else
                            {
                                field.Name = typed;
                                changed = true;
                            }
                        }
                    }
                }

                BeginInspectorProperty("Type");
                {
                    int typeIndex = static_cast<int>(ScriptFields::GetType(field.DefaultValue));
                    const float reserved = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x;
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - reserved);

                    if (ImGui::Combo("##Type", &typeIndex, TypeItems.data(), static_cast<int>(TypeItems.size())))
                    {
                        // The value cannot survive a type change: a string is not a float, and the
                        // stored variant has to hold the type the field now declares.
                        const ScriptFieldData reset = ScriptFields::MakeDefaultValue(
                            static_cast<ScriptFieldType>(typeIndex));
                        field.DefaultValue = reset;
                        field.Value = reset;
                        changed = true;
                    }

                    // A running instance was built from the list it started with, so its shape is
                    // fixed until the game stops; only the values are live.
                    ImGui::SameLine();
                    ImGui::BeginDisabled(editRuntimeValues);
                    if (DrawRemoveButton(editRuntimeValues
                            ? "Stop the game to remove a field"
                            : "Remove field"))
                    {
                        removeIndex = index;
                    }
                    ImGui::EndDisabled();
                }

                BeginInspectorProperty("Value");
                {
                    ScriptFieldData& target = editRuntimeValues ? field.Value : field.DefaultValue;
                    if (DrawFieldValue(target))
                    {
                        if (!editRuntimeValues)
                        {
                            // Outside Play the two hold the same value, so the scene is ready to run
                            // the moment Play is pressed.
                            field.Value = field.DefaultValue;
                        }
                        changed = true;
                    }
                }

                if (editRuntimeValues && !ScriptFields::IsDefaultValue(field))
                {
                    // The design-time value is what the scene stores, so a row the script changed
                    // says so instead of looking like an edit that will be saved.
                    BeginInspectorProperty("Runtime");
                    ImGui::TextDisabled("changed by the script");
                }

                ImGui::EndTable();
            }

            ImGui::PopID();
        }

        if (removeIndex < fields.size())
        {
            fields.erase(fields.begin() + static_cast<ptrdiff_t>(removeIndex));
            changed = true;
        }

        if (!editRuntimeValues)
        {
            if (ImGui::Button("Add Field", GetFullWidthButtonSize()))
            {
                fields.push_back(MakeNewField(fields));
                changed = true;
            }
        }
        else if (fields.empty())
        {
            ImGui::TextDisabled("No fields");
        }

        return changed;
    }
}
