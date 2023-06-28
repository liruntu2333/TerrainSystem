#pragma once
#include <directxtk/SimpleMath.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <filesystem>
#include <directxtk/DirectXHelpers.h>

#include "Vertex.h"

namespace DirectX
{
    class ClipmapTexture;
}

class HeightMap;
static constexpr int ClipmapK = 8;
static constexpr int ClipmapN = (1 << ClipmapK) - 1;
static constexpr int ClipmapM = (ClipmapN + 1) / 4;

class ClipmapLevelBase
{
public:
    struct HollowRing
    {
        GridInstance Blocks[12];
        GridInstance RingFixUp;
        GridInstance Trim;
        int TrimId;
    };

    struct SolidSquare
    {
        GridInstance Blocks[16];
        GridInstance RingFixUp;
        GridInstance Trim[2];
        int TrimId[2];
    };

    static void LoadFootprintGeometry(const std::filesystem::path& path, ID3D11Device* device);

    friend class TerrainSystem;

protected:
    inline static Microsoft::WRL::ComPtr<ID3D11Buffer> BlockVb = nullptr;
    inline static Microsoft::WRL::ComPtr<ID3D11Buffer> BlockIb = nullptr;
    inline static Microsoft::WRL::ComPtr<ID3D11Buffer> RingFixUpVb = nullptr;
    inline static Microsoft::WRL::ComPtr<ID3D11Buffer> RingFixUpIb = nullptr;
    inline static Microsoft::WRL::ComPtr<ID3D11Buffer> TrimVb[4] { nullptr };
    inline static Microsoft::WRL::ComPtr<ID3D11Buffer> TrimIb[4] = { nullptr };
    inline static int BlockIdxCnt;
    inline static int RingIdxCnt;
    inline static int TrimIdxCnt;
};

class ClipmapLevel : public ClipmapLevelBase
{
public:
    ClipmapLevel(
        int l, const std::shared_ptr<HeightMap>& src,
        const std::shared_ptr<DirectX::ClipmapTexture>& hTex);
    ~ClipmapLevel() = default;

    void UpdateOffset(const DirectX::SimpleMath::Vector2& dView);
    void UpdateTexture(ID3D11DeviceContext* context);
    [[nodiscard]] HollowRing GetHollowRing() const;
    [[nodiscard]] SolidSquare GetSolidSquare() const;
    [[nodiscard]] float GetHeight() const;

    friend TerrainSystem;

protected:
    const int m_Level;
    const float m_GridSpacing;
    inline static constexpr int TextureSz = 1 << ClipmapK;
    inline static constexpr float OneOverSz = 1.0f / TextureSz;
    inline static constexpr int TextureN = (1 << ClipmapK) - 1; // 1 row 1 col left invalid
    inline static constexpr int TextureScaleHeight = 1;
    inline static constexpr int TextureScaleNormal = 1;
    inline static constexpr int TextureScaleAlbedo = 1;

    DirectX::XMINT2 m_MappedOrigin =
    {
        std::numeric_limits<int>::min(), std::numeric_limits<int>::min()
    };                                  // xy offset in source height texel space
    DirectX::XMINT2 m_GridOrigin;       // * GridSpacing to get world offset
    DirectX::XMINT2 m_TexelOrigin;      // * TextureSpacing to get texture offset

    DirectX::SimpleMath::Vector2 m_Ticker;
    std::shared_ptr<HeightMap> m_HeightSrc;
    std::shared_ptr<DirectX::ClipmapTexture> m_HeightTex;
    const int m_ArraySlice;
};
