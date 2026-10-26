// Script field tests.
//
// A script field is plain data: a name, a type carried by its value, a design-time value and a
// runtime copy. Its persistence is YAML produced by the same helpers the scene serializer calls.
// Neither needs a window, a GPU resource or a Lua state, so the whole suite runs headless.

#include <doctest/doctest.h>

#include "Scripting/ScriptFields.h"

#include <yaml-cpp/yaml.h>

#include <string>
#include <vector>

using namespace HachimiEngine;

namespace
{
    ScriptField MakeField(const std::string& name, const ScriptFieldData& value)
    {
        ScriptField field;
        field.Name = name;
        field.Value = value;
        field.DefaultValue = value;
        return field;
    }

    std::vector<ScriptField> ReadBack(const std::vector<ScriptField>& fields)
    {
        YAML::Emitter out;
        ScriptFields::EmitList(out, fields);
        REQUIRE(out.good());
        return ScriptFields::ReadList(YAML::Load(out.c_str()));
    }

    std::vector<ScriptField> ReadText(const std::string& text)
    {
        return ScriptFields::ReadList(YAML::Load(text));
    }
}

TEST_SUITE("Scripting")
{
    TEST_CASE("a field's type is the type of the value it holds")
    {
        constexpr ScriptFieldType AllTypes[] {
            ScriptFieldType::Bool, ScriptFieldType::Int, ScriptFieldType::Float, ScriptFieldType::String,
            ScriptFieldType::Vec3
        };

        for (const ScriptFieldType type : AllTypes)
        {
            const ScriptFieldData data = ScriptFields::MakeDefaultValue(type);
            CHECK(ScriptFields::GetType(data) == type);

            // The name in the file is what the type round trips through, so both directions are
            // part of the same contract.
            ScriptFieldType parsed = ScriptFieldType::String;
            REQUIRE(ScriptFields::ParseTypeName(ScriptFields::GetTypeName(type), parsed));
            CHECK(parsed == type);
        }

        CHECK(std::get<bool>(ScriptFields::MakeDefaultValue(ScriptFieldType::Bool)) == false);
        CHECK(std::get<int64_t>(ScriptFields::MakeDefaultValue(ScriptFieldType::Int)) == 0);
        CHECK(std::get<float>(ScriptFields::MakeDefaultValue(ScriptFieldType::Float)) == 0.0f);
        CHECK(std::get<std::string>(ScriptFields::MakeDefaultValue(ScriptFieldType::String)).empty());
    }

    TEST_CASE("an unknown type name is refused instead of guessed")
    {
        ScriptFieldType type = ScriptFieldType::Bool;
        CHECK_FALSE(ScriptFields::ParseTypeName("Vector3", type));
        CHECK_FALSE(ScriptFields::ParseTypeName("bool", type));
        CHECK_FALSE(ScriptFields::ParseTypeName("", type));
    }

    TEST_CASE("values of every type survive a YAML round trip")
    {
        const std::vector<ScriptField> fields {
            MakeField("Enabled", true),
            MakeField("Count", int64_t { 7 }),
            MakeField("Speed", 2.5f),
            MakeField("Label", std::string("hello world")),
            MakeField("Offset", Math::Vec3(1.0f, -2.0f, 3.5f))
        };

        const std::vector<ScriptField> loaded = ReadBack(fields);
        REQUIRE(loaded.size() == fields.size());

        CHECK(loaded[0].Name == "Enabled");
        CHECK(loaded[0].DefaultValue == ScriptFieldData { true });
        CHECK(loaded[1].DefaultValue == ScriptFieldData { int64_t { 7 } });
        CHECK(loaded[2].DefaultValue == ScriptFieldData { 2.5f });
        CHECK(loaded[3].DefaultValue == ScriptFieldData { std::string("hello world") });
        CHECK(loaded[4].DefaultValue == ScriptFieldData { Math::Vec3(1.0f, -2.0f, 3.5f) });
    }

    TEST_CASE("a scene file never carries the runtime value")
    {
        ScriptField field = MakeField("Health", 100.0f);
        field.Value = 12.0f;
        CHECK_FALSE(ScriptFields::IsDefaultValue(field));

        // What the script wrote is not persisted, so playing a scene once cannot become the value
        // the next session starts from.
        const std::vector<ScriptField> loaded = ReadBack({ field });
        CHECK_FALSE(loaded.empty());

        YAML::Emitter out;
        ScriptFields::Emit(out, field);
        const std::string text = out.c_str();
        CHECK(text.find("100") != std::string::npos);
        CHECK(text.find("12") == std::string::npos);

        REQUIRE(loaded.size() == 1);
        CHECK(loaded[0].DefaultValue == ScriptFieldData { 100.0f });
        CHECK(loaded[0].Value == ScriptFieldData { 100.0f });
        CHECK(ScriptFields::IsDefaultValue(loaded[0]));
    }

    TEST_CASE("an unknown type falls back to the type of the default value")
    {
        const std::vector<ScriptField> fields = ReadText(
            "- Name: Target\n"
            "  Type: Entity\n"
            "  DefaultValue: [1, 2, 3]\n");

        REQUIRE(fields.size() == 1);
        CHECK(ScriptFields::GetType(fields[0].DefaultValue) == ScriptFieldType::Vec3);
        CHECK(fields[0].DefaultValue == ScriptFieldData { Math::Vec3(1.0f, 2.0f, 3.0f) });
    }

    TEST_CASE("a missing type is inferred, and an unreadable one drops the field")
    {
        const std::vector<ScriptField> inferred = ReadText(
            "- Name: Speed\n"
            "  DefaultValue: 3.5\n");
        REQUIRE(inferred.size() == 1);
        CHECK(ScriptFields::GetType(inferred[0].DefaultValue) == ScriptFieldType::Float);

        // No type and nothing to infer one from: inventing a Float here would hide a file that
        // this build cannot read.
        const std::vector<ScriptField> dropped = ReadText(
            "- Name: Mystery\n"
            "  Type: Whatever\n"
            "  DefaultValue: { a: 1 }\n");
        CHECK(dropped.empty());
    }

    TEST_CASE("a default that is not the declared type falls back to the zero value")
    {
        const std::vector<ScriptField> fields = ReadText(
            "- Name: Speed\n"
            "  Type: Float\n"
            "  DefaultValue: fast\n");

        REQUIRE(fields.size() == 1);
        CHECK(fields[0].DefaultValue == ScriptFieldData { 0.0f });
    }

    TEST_CASE("a nameless or duplicated field is refused")
    {
        const std::vector<ScriptField> fields = ReadText(
            "- Type: Float\n"
            "  DefaultValue: 1.0\n"
            "- Name: Speed\n"
            "  Type: Float\n"
            "  DefaultValue: 2.0\n"
            "- Name: Speed\n"
            "  Type: Float\n"
            "  DefaultValue: 3.0\n");

        REQUIRE(fields.size() == 1);
        CHECK(fields[0].Name == "Speed");
        CHECK(fields[0].DefaultValue == ScriptFieldData { 2.0f });
    }

    TEST_CASE("a missing or malformed field list reads as no fields")
    {
        CHECK(ScriptFields::ReadList(YAML::Node()).empty());
        CHECK(ScriptFields::ReadList(YAML::Load("Fields: 3")["Fields"]).empty());
    }

    TEST_CASE("lookup is by exact name and keeps the first match")
    {
        std::vector<ScriptField> fields { MakeField("Speed", 1.0f), MakeField("speed", 2.0f) };

        REQUIRE(ScriptFields::Find(fields, "Speed") != nullptr);
        CHECK(ScriptFields::Find(fields, "Speed") == &fields[0]);
        CHECK(ScriptFields::Find(fields, "speed") == &fields[1]);
        CHECK(ScriptFields::Find(fields, "Velocity") == nullptr);

        const std::vector<ScriptField>& constFields = fields;
        CHECK(ScriptFields::Find(constFields, "speed") == &constFields[1]);
    }
}
