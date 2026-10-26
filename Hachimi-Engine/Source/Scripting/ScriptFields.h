#pragma once

#include "Core/Base.h"
#include "Math/Math.h"

#include <yaml-cpp/yaml.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace HachimiEngine
{
    // Value type of one script field.
    //
    // The order matches the alternatives of ScriptFieldData, so a field's type is a function of
    // the value it holds instead of a second member that can disagree with it.
    enum class ScriptFieldType
    {
        Bool = 0,
        Int = 1,
        Float = 2,
        String = 3,
        Vec3 = 4
    };

    using ScriptFieldData = std::variant<bool, int64_t, float, std::string, Math::Vec3>;

    // One author-facing value on one script slot.
    //
    // DefaultValue is the design-time value: the inspector edits it and the scene file stores it.
    // Value is the copy the running script mutates through self.fields, and it is never
    // serialized, so a Play session cannot write runtime state back into the scene.
    struct ScriptField
    {
        std::string Name;
        ScriptFieldData Value;
        ScriptFieldData DefaultValue;

        // Spelled out instead of relying on the implicit memberwise operator: this compiler does
        // not declare one for a class holding a std::variant, and the editor's undo command
        // compares whole field lists to decide whether an edit happened at all.
        friend bool operator==(const ScriptField& lhs, const ScriptField& rhs)
        {
            return lhs.Name == rhs.Name && lhs.Value == rhs.Value && lhs.DefaultValue == rhs.DefaultValue;
        }
    };

    // Field helpers shared by the scene layer (persistence), the Lua layer (self.fields) and the
    // editor (the field table). Nothing here knows about Lua, which is why the scene component can
    // own fields without depending on a scripting backend.
    namespace ScriptFields
    {
        // The name stored in the .hscene file: "Bool", "Int", "Float", "String", "Vec3".
        std::string_view GetTypeName(ScriptFieldType type);
        // False for a name this build does not know, which the caller reports instead of guessing.
        bool ParseTypeName(std::string_view name, ScriptFieldType& outType);

        ScriptFieldType GetType(const ScriptFieldData& data);
        // Zero value of a type: false, 0, 0.0, "", (0,0,0).
        ScriptFieldData MakeDefaultValue(ScriptFieldType type);
        // True when both hold the same alternative. Variants of different types never compare
        // equal, so this is what keeps a mistyped assignment from silently converting.
        bool HasSameType(const ScriptFieldData& lhs, const ScriptFieldData& rhs);

        // Names are Lua identifiers the script indexes self.fields with, so they are compared
        // exactly. FindOrAdd keeps the first field of a name and ignores a duplicate.
        ScriptField* Find(std::vector<ScriptField>& fields, std::string_view name);
        const ScriptField* Find(const std::vector<ScriptField>& fields, std::string_view name);

        // True when Value still holds the design-time value. During Play this is false as soon as
        // a script writes the field, which is what the editor shows as a runtime override.
        bool IsDefaultValue(const ScriptField& field);

        // Persistence of one script slot's fields. A field is a map of Name, Type and
        // DefaultValue; the runtime value is never written, so loading a scene always starts from
        // the design-time value the inspector shows.
        void Emit(YAML::Emitter& out, const ScriptField& field);
        void EmitList(YAML::Emitter& out, const std::vector<ScriptField>& fields);
        // Reads a field list. An entry that cannot be understood is reported and skipped rather
        // than invented: an unknown type falls back to the type of its default value when that is
        // readable, and to nothing when it is not.
        std::vector<ScriptField> ReadList(const YAML::Node& node);
    }
}
