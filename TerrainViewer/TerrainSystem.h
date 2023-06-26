#pragma once

#include <map>

#include "ClipmapLevel.h"
#include "Patch.h"

#include "Texture2D.h"
#include "../HeightMapSplitter/BoundTree.h"

#include "HeightMap.h"

class TerrainSystem
{
public:
    struct PatchRenderResource
    {
        ID3D11ShaderResourceView* Height {};
        ID3D11ShaderResourceView* Normal {};
        ID3D11ShaderResourceView* Albedo {};
        std::vector<Patch::RenderResource> Patches {};

        PatchRenderResource() = default;
    };

    struct ClipmapRenderResource
    {
        ID3D11ShaderResourceView* Height {};
        ID3D11ShaderResourceView* Normal {};
        ID3D11ShaderResourceView* Albedo {};

        ID3D11Buffer* BlockVb = nullptr;
        ID3D11Buffer* BlockIb = nullptr;
        ID3D11Buffer* RingVb = nullptr;
        ID3D11Buffer* RingIb = nullptr;
        ID3D11Buffer* TrimVb[4] { nullptr };
        ID3D11Buffer* TrimIb[4] = { nullptr };

        int BlockIdxCnt = 0;
        int RingIdxCnt = 0;
        int TrimIdxCnt = 0;

        std::vector<GridInstance> Grids {};
        int BlockInstanceStart = 0;
        int RingInstanceStart = 0;
        int TrimInstanceStart[4] {};

        ClipmapRenderResource() = default;
    };

    TerrainSystem(std::filesystem::path path, ID3D11Device* device);
    ~TerrainSystem() = default;

    [[nodiscard]] PatchRenderResource GetPatchResources(
        const DirectX::XMINT2& camXyForCull,
        const DirectX::BoundingFrustum& frustumLocal, float yScale,
        std::vector<DirectX::BoundingBox>& bbs, ID3D11Device* device) const;

    [[nodiscard]] ClipmapRenderResource GetClipmapResources(
        const DirectX::SimpleMath::Vector3& view3,
        const DirectX::SimpleMath::Vector3& dView, float yScale);

    static constexpr int LevelCount = 9;
    static constexpr int LevelMin = 0;

protected:
    void InitMeshPatches(ID3D11Device* device);
    void InitClipmapLevels(ID3D11Device* device);

    std::map<int, std::shared_ptr<Patch>> m_Patches {};
    std::unique_ptr<BoundTree> m_BoundTree = nullptr;

    std::vector<ClipmapLevel> m_Levels {};
    std::unique_ptr<HeightMap> m_HeightMap = nullptr;

    std::unique_ptr<DirectX::Texture2D> m_Height {};
    std::unique_ptr<DirectX::Texture2D> m_Normal {};
    std::unique_ptr<DirectX::Texture2D> m_Albedo {};

    const std::filesystem::path m_Path;

    DirectX::SimpleMath::Vector2 m_View {};
};
