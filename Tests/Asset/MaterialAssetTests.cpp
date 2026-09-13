// MaterialAsset: the document behind Assets/Materials/*.hmaterial.
//
// A material is deliberately a plain value with a YAML document: it holds asset references, not
// loaded objects, so it can be read and written with no OpenGL context and is therefore covered
// here rather than by hand.

#include <doctest/doctest.h>

#include "Asset/AssetHandle.h"
#include "Asset/MaterialAsset.h"
#include "Core/UUID.h"

#include <string>

using namespace HachimiEngine;

TEST_SUITE_BEGIN("Asset");

TEST_CASE("a default material names the engine shader and is opaque")
{
    const MaterialAsset material = MaterialAsset::Default();

    CHECK(std::string(material.Shader) == std::string(MaterialAsset::DefaultShaderName));
    CHECK_FALSE(material.AlbedoTexture.IsValid());
    CHECK(material.BaseColor.w == doctest::Approx(1.0f));
    CHECK(material.Roughness > 0.0f);
    CHECK(material.Roughness <= 1.0f);
    CHECK(material.Metallic >= 0.0f);
    CHECK(material.Metallic <= 1.0f);
}

TEST_CASE("a material round trips through its document")
{
    MaterialAsset material;
    material.Shader = "Default.glsl";
    material.BaseColor = { 0.1f, 0.2f, 0.3f, 0.4f };
    material.Roughness = 0.25f;
    material.Metallic = 0.75f;
    material.AlbedoTexture = AssetHandle::From(UUID(0x0123456789ABCDEFull), AssetType::Texture);

    MaterialAsset read;
    REQUIRE(MaterialAsset::Deserialize(MaterialAsset::Serialize(material), read));

    CHECK(read.Shader == material.Shader);
    CHECK(read.BaseColor == material.BaseColor);
    CHECK(read.Roughness == doctest::Approx(material.Roughness));
    CHECK(read.Metallic == doctest::Approx(material.Metallic));
    REQUIRE(read.AlbedoTexture.IsValid());
    CHECK(read.AlbedoTexture.ID == material.AlbedoTexture.ID);
    // A material reading a texture reference always produces a texture handle, never any other kind.
    CHECK(read.AlbedoTexture.Type == AssetType::Texture);
}

TEST_CASE("a material with no texture omits the reference entirely")
{
    const MaterialAsset material = MaterialAsset::Default();
    const std::string document = MaterialAsset::Serialize(material);

    CHECK(document.find("AlbedoTexture") == std::string::npos);
    CHECK(document.find("Material:") != std::string::npos);
}

TEST_CASE("missing fields keep their defaults and unknown fields are ignored")
{
    MaterialAsset read;
    REQUIRE(MaterialAsset::Deserialize("Material:\n  BaseColor: [0.5, 0.5, 0.5, 1.0]\n", read));

    // What the document did not say keeps the value a fresh material has, so a hand-written or
    // newer file still loads instead of failing.
    CHECK(read.Shader == MaterialAsset::DefaultShaderName);
    CHECK(read.Roughness == doctest::Approx(MaterialAsset::Default().Roughness));
    CHECK(read.BaseColor.x == doctest::Approx(0.5f));

    MaterialAsset tolerant;
    CHECK(MaterialAsset::Deserialize("Material:\n  Shader: Default.glsl\n  Emission: 4.0\n", tolerant));
}

TEST_CASE("out of range values are clamped rather than trusted")
{
    MaterialAsset read;
    REQUIRE(MaterialAsset::Deserialize(
        "Material:\n  BaseColor: [4.0, -2.0, 0.5, 1.0]\n  Roughness: 12.0\n  Metallic: -3.0\n", read));

    CHECK(read.BaseColor.x == doctest::Approx(1.0f));
    CHECK(read.BaseColor.y == doctest::Approx(0.0f));
    CHECK(read.BaseColor.z == doctest::Approx(0.5f));
    CHECK(read.Roughness == doctest::Approx(1.0f));
    CHECK(read.Metallic == doctest::Approx(0.0f));
}

TEST_CASE("an unreadable texture reference becomes no texture")
{
    MaterialAsset read;
    REQUIRE(MaterialAsset::Deserialize("Material:\n  AlbedoTexture: \"not-a-uuid\"\n", read));
    CHECK_FALSE(read.AlbedoTexture.IsValid());

    MaterialAsset zeroed;
    REQUIRE(MaterialAsset::Deserialize("Material:\n  AlbedoTexture: \"0000000000000000\"\n", zeroed));
    CHECK_FALSE(zeroed.AlbedoTexture.IsValid());
}

TEST_CASE("a document that is not a material is refused")
{
    MaterialAsset read;
    CHECK_FALSE(MaterialAsset::Deserialize("", read));
    CHECK_FALSE(MaterialAsset::Deserialize("[1, 2, 3]\n", read));
    // Malformed YAML is reported, not silently turned into defaults.
    CHECK_FALSE(MaterialAsset::Deserialize("Material: [unclosed\n", read));
}

TEST_SUITE_END();
