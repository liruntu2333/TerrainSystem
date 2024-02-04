#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <directxtk/SimpleMath.h>

// Remember to change in shader as well.
struct VertexPositionNormalTangentTexture
{
    DirectX::SimpleMath::Vector3 Pos;
    DirectX::SimpleMath::Vector3 Nor;
    DirectX::SimpleMath::Vector3 Tan;
    DirectX::SimpleMath::Vector2 Tc;
    //float Padding;

    VertexPositionNormalTangentTexture() = default;

    VertexPositionNormalTangentTexture(
        const DirectX::SimpleMath::Vector3& pos,
        const DirectX::SimpleMath::Vector3& nor,
        const DirectX::SimpleMath::Vector3& tan,
        const DirectX::SimpleMath::Vector2& tc)
        : Pos(pos), Nor(nor), Tan(tan), Tc(tc) {}
};

struct ImporterMeshData
{
    std::vector<VertexPositionNormalTangentTexture> Vertices {};
    std::vector<uint32_t> Indices {};
    uint32_t MaterialIndex {};

    ImporterMeshData() = default;

    ImporterMeshData(const std::vector<VertexPositionNormalTangentTexture>& vertices, const std::vector<uint32_t>& indices, uint32_t materialIndex)
        : Vertices(vertices), Indices(indices), MaterialIndex(materialIndex) {}

    ImporterMeshData(std::vector<VertexPositionNormalTangentTexture>&& vertices, std::vector<uint32_t>&& indices, uint32_t materialIndex)
        : Vertices(std::move(vertices)), Indices(std::move(indices)), MaterialIndex(materialIndex) {}
};

struct ImporterMaterialData
{
    std::string Name {};
    std::filesystem::path TexturePath;

    ImporterMaterialData() = default;

    ImporterMaterialData(std::string name, std::filesystem::path texturePath)
        : Name(std::move(name)), TexturePath(std::move(texturePath)) {}
};

class AssetImporter
{
public:
    struct ImporterModelData
    {
        std::vector<ImporterMeshData> Subsets {};
        std::vector<ImporterMaterialData> Materials {};
        DirectX::BoundingBox BoundingBox {};
        float AreaSum = 0;

        ImporterModelData()                    = default;
        ImporterModelData(ImporterModelData&&) = default;
    };

    static ImporterModelData LoadAsset(const std::filesystem::path& fPath, bool yToZ = false);
};
