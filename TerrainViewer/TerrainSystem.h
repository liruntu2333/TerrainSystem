#pragma once

#include <map>

#include "ClipmapLevel.h"
#include "Patch.h"

#include "Texture2D.h"
#include "../HeightMapSplitter/BoundTree.h"

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
        // ID3D11ShaderResourceView* Height {};
        ID3D11ShaderResourceView* HeightCm {};
        ID3D11ShaderResourceView* NormalCm {};
        ID3D11ShaderResourceView* AlbedoCm {};

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

    TerrainSystem(
        const DirectX::SimpleMath::Vector3& view,
        std::filesystem::path path,
        ID3D11Device* device);
    ~TerrainSystem() = default;

    void ResetClipmapTexture();

    [[nodiscard]] PatchRenderResource GetPatchResources(
        const DirectX::XMINT2& camXyForCull,
        const DirectX::BoundingFrustum& frustumLocal, float yScale,
        std::vector<DirectX::BoundingBox>& bbs, ID3D11Device* device) const;

    [[nodiscard]] ClipmapRenderResource TickClipmap(
        const DirectX::BoundingFrustum& frustum,
        float yScale, ID3D11DeviceContext* context, const int blendMode);

    [[nodiscard]] const DirectX::Texture2D& GetHeightClip() const { return *m_HeightCm; }
    [[nodiscard]] const DirectX::Texture2D& GetAlbedoClip() const { return *m_AlbedoCm; }
    [[nodiscard]] const DirectX::Texture2D& GetNormalClip() const { return *m_NormalCm; }

    static constexpr int LevelCount = 8;
    static constexpr float LevelZeroScale = 1.0f; // 1 m per grid
    
protected:
    void InitMeshPatches(ID3D11Device* device);
    void InitClipTextures(ID3D11Device* device);
    void InitClipmapLevels(ID3D11Device* device, const DirectX::SimpleMath::Vector3& view);
    [[nodiscard]] ClipmapRenderResource GetClipmapRenderResource() const;

    std::map<int, std::shared_ptr<Patch>> m_Patches {};
    std::unique_ptr<BoundTree> m_BoundTree = nullptr;

    std::vector<ClipmapLevel> m_Levels {};
    std::shared_ptr<DirectX::ClipmapTexture> m_HeightCm {};
    std::shared_ptr<DirectX::ClipmapTexture> m_AlbedoCm {};
    std::shared_ptr<DirectX::ClipmapTexture> m_NormalCm {};

    // std::unique_ptr<DirectX::Texture2D> m_Height {};
    // std::unique_ptr<DirectX::Texture2D> m_Normal {};
    // std::unique_ptr<DirectX::Texture2D> m_Albedo {};

    const std::filesystem::path m_Path;
};
