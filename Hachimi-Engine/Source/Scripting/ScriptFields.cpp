#include "Scripting/ScriptFields.h"

#include "Core/Log.h"

#include <array>
#include <optional>

namespace HachimiEngine
{
    namespace
    {
        constexpr std::array<std::string_view, 5> TypeNames { "Bool", "Int", "Float", "String", "Vec3" };

        void EmitData(YAML::Emitter& out, const ScriptFieldData& data)
        {
            std::visit([&out](const auto& value)
            {
                using TValue = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<TValue, Math::Vec3>)
                {
                    out << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << YAML::EndSeq;
                }
                else
                {
                    out << value;
                }
            }, data);
        }

        // Reads one value of a known type. Returns false when the node cannot be understood as
        // that type, which the caller reports instead of converting silently.
        bool ReadData(const YAML::Node& node, ScriptFieldType type, ScriptFieldData& outData)
        {
            try
            {
                switch (type)
                {
                    case ScriptFieldType::Bool:
                        outData = node.as<bool>();
                        return true;
                    case ScriptFieldType::Int:
                        outData = node.as<int64_t>();
                        return true;
                    case ScriptFieldType::Float:
                        outData = node.as<float>();
                        return true;
                    case ScriptFieldType::String:
                        outData = node.as<std::string>();
                        return true;
                    case ScriptFieldType::Vec3:
                    {
                        if (!node.IsSequence() || node.size() < 3)
                        {
                            return false;
                        }
                        outData = Math::Vec3(node[0].as<float>(), node[1].as<float>(), node[2].as<float>());
                        return true;
                    }
                }
            }
            catch (const YAML::Exception&)
            {
                return false;
            }
            catch (const std::exception&)
            {
                return false;
            }

            return false;
        }

        // Guesses a field's type from the shape of its default value. Used only when the stored
        // type name is missing or unknown, so a scene written by a newer build still loads the
        // field it can still read instead of dropping it.
        std::optional<ScriptFieldType> InferType(const YAML::Node& node)
        {
            if (!node)
            {
                return std::nullopt;
            }

            if (node.IsSequence())
            {
                return node.size() >= 3 ? std::optional(ScriptFieldType::Vec3) : std::nullopt;
            }

            // Scalars are tried narrowest first: a YAML integer also converts to float and string,
            // so the other order would turn every number into a string.
            ScriptFieldData ignored;
            constexpr std::array<ScriptFieldType, 4> Candidates {
                ScriptFieldType::Bool, ScriptFieldType::Int, ScriptFieldType::Float, ScriptFieldType::String
            };

            for (const ScriptFieldType candidate : Candidates)
            {
                if (ReadData(node, candidate, ignored))
                {
                    return candidate;
                }
            }

            return std::nullopt;
        }
    }

    namespace ScriptFields
    {
        std::string_view GetTypeName(ScriptFieldType type)
        {
            const size_t index = static_cast<size_t>(type);
            return index < TypeNames.size() ? TypeNames[index] : std::string_view("Bool");
        }

        bool ParseTypeName(std::string_view name, ScriptFieldType& outType)
        {
            for (size_t index = 0; index < TypeNames.size(); ++index)
            {
                if (TypeNames[index] == name)
                {
                    outType = static_cast<ScriptFieldType>(index);
                    return true;
                }
            }
            return false;
        }

        ScriptFieldType GetType(const ScriptFieldData& data)
        {
            return static_cast<ScriptFieldType>(data.index());
        }

        ScriptFieldData MakeDefaultValue(ScriptFieldType type)
        {
            switch (type)
            {
                case ScriptFieldType::Bool:
                    return false;
                case ScriptFieldType::Int:
                    return int64_t { 0 };
                case ScriptFieldType::Float:
                    return 0.0f;
                case ScriptFieldType::String:
                    return std::string();
                case ScriptFieldType::Vec3:
                    return Math::Vec3(0.0f);
            }

            return false;
        }

        bool HasSameType(const ScriptFieldData& lhs, const ScriptFieldData& rhs)
        {
            return lhs.index() == rhs.index();
        }

        ScriptField* Find(std::vector<ScriptField>& fields, std::string_view name)
        {
            for (ScriptField& field : fields)
            {
                if (field.Name == name)
                {
                    return &field;
                }
            }
            return nullptr;
        }

        const ScriptField* Find(const std::vector<ScriptField>& fields, std::string_view name)
        {
            for (const ScriptField& field : fields)
            {
                if (field.Name == name)
                {
                    return &field;
                }
            }
            return nullptr;
        }

        bool IsDefaultValue(const ScriptField& field)
        {
            return field.Value == field.DefaultValue;
        }

        void Emit(YAML::Emitter& out, const ScriptField& field)
        {
            // The type is written from the default value, which is the field's authoritative
            // storage: a runtime value that somehow ended up with another type must not be able to
            // rewrite the field's type when the scene is saved.
            out << YAML::BeginMap;
            out << YAML::Key << "Name" << YAML::Value << field.Name;
            out << YAML::Key << "Type" << YAML::Value << std::string(GetTypeName(GetType(field.DefaultValue)));
            out << YAML::Key << "DefaultValue" << YAML::Value;
            EmitData(out, field.DefaultValue);
            // Value is deliberately absent: it is the runtime copy a running script writes, and
            // loading a scene must always start from the design-time value.
            out << YAML::EndMap;
        }

        void EmitList(YAML::Emitter& out, const std::vector<ScriptField>& fields)
        {
            out << YAML::BeginSeq;
            for (const ScriptField& field : fields)
            {
                Emit(out, field);
            }
            out << YAML::EndSeq;
        }

        std::vector<ScriptField> ReadList(const YAML::Node& node)
        {
            std::vector<ScriptField> fields;
            if (!node)
            {
                return fields;
            }

            if (!node.IsSequence())
            {
                HE_CORE_ERROR("Script fields must be a sequence; the value in the scene file is ignored");
                return fields;
            }

            for (const YAML::Node fieldNode : node)
            {
                if (!fieldNode.IsMap())
                {
                    HE_CORE_ERROR("Skipped a script field that is not a map");
                    continue;
                }

                ScriptField field;
                field.Name = fieldNode["Name"].as<std::string>("");
                if (field.Name.empty())
                {
                    HE_CORE_ERROR("Skipped a script field without a name");
                    continue;
                }

                const YAML::Node defaultNode = fieldNode["DefaultValue"];
                ScriptFieldType type = ScriptFieldType::Float;

                const std::string typeName = fieldNode["Type"].as<std::string>("");
                if (typeName.empty())
                {
                    const std::optional<ScriptFieldType> inferred = InferType(defaultNode);
                    if (!inferred.has_value())
                    {
                        HE_CORE_ERROR("Script field '{}' has no type and no readable default value; the field is not loaded",
                            field.Name);
                        continue;
                    }
                    type = *inferred;
                    HE_CORE_WARN("Script field '{}' has no type; using '{}' inferred from its default value",
                        field.Name, GetTypeName(type));
                }
                else if (!ParseTypeName(typeName, type))
                {
                    const std::optional<ScriptFieldType> inferred = InferType(defaultNode);
                    if (!inferred.has_value())
                    {
                        HE_CORE_ERROR("Script field '{}' has unknown type '{}' and no readable default value; the field is not loaded",
                            field.Name, typeName);
                        continue;
                    }
                    type = *inferred;
                    HE_CORE_ERROR("Script field '{}' has unknown type '{}'; falling back to '{}' from its default value",
                        field.Name, typeName, GetTypeName(type));
                }

                field.DefaultValue = MakeDefaultValue(type);
                if (defaultNode && !ReadData(defaultNode, type, field.DefaultValue))
                {
                    HE_CORE_ERROR("Script field '{}' has a default value that is not a '{}'; using the type's zero value",
                        field.Name, GetTypeName(type));
                    field.DefaultValue = MakeDefaultValue(type);
                }

                // The runtime copy always starts as the design-time value; a scene file has no say
                // in it, which is what keeps a saved scene from carrying a previous Play session.
                field.Value = field.DefaultValue;

                if (Find(fields, field.Name) != nullptr)
                {
                    HE_CORE_ERROR("Script field '{}' appears twice; the second entry is ignored", field.Name);
                    continue;
                }

                fields.push_back(std::move(field));
            }

            return fields;
        }
    }
}
