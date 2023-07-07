#pragma once
#include <directxtk/SimpleMath.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <filesystem>

#include "Vertex.h"

namespace DirectX
{
    class ClipmapTexture;
    class HeightMap;
}

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
        GridInstance Trim[2]; // lt, rb
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
        unsigned l, float gScl, const DirectX::SimpleMath::Vector2& view,
        const std::shared_ptr<DirectX::HeightMap>& src,
        const std::shared_ptr<DirectX::ClipmapTexture>& hTex);
    ~ClipmapLevel() = default;

    void UpdateOffset(
        const DirectX::SimpleMath::Vector2& dView, const DirectX::SimpleMath::Vector2& view,
        const DirectX::SimpleMath::Vector2& ofsFiner);
    void UpdateTexture(ID3D11DeviceContext* context);
    [[nodiscard]] HollowRing GetHollowRing(const DirectX::SimpleMath::Vector2& toc) const;
    [[nodiscard]] SolidSquare GetSolidSquare(const DirectX::SimpleMath::Vector2& toc) const;
    [[nodiscard]] float GetHeight() const;
    [[nodiscard]] DirectX::SimpleMath::Vector2 GetFinerOffset() const;

    [[nodiscard]] DirectX::SimpleMath::Vector2 GetWorldOffset() const
    {
        return m_GridOrigin * m_GridSpacing;
    }

    [[nodiscard]] bool IsActive(float hView, float hScale) const
    {
        return std::abs(hView - hScale * GetHeight()) < 2.5f * 254.0f * m_GridSpacing;
    }

    friend class TerrainSystem;

protected:
    [[nodiscard]] int GetTrimPattern(const DirectX::SimpleMath::Vector2& finer) const
    {
        const auto ofs = (finer - GetWorldOffset()) / m_GridSpacing;
        const int ox = ofs.x;
        const int oy = ofs.y;
        if (ox == ClipmapM && oy == ClipmapM) return 0;
        if (ox == ClipmapM - 1 && oy == ClipmapM) return 1;
        if (ox == ClipmapM - 1 && oy == ClipmapM - 1) return 2;
        /*if (ox == ClipmapM && oy == ClipmapM - 1)*/
        return 3;
    }

    const unsigned m_Level;
    const float m_GridSpacing;
    inline static constexpr int TextureSz = 1 << ClipmapK;
    inline static constexpr float OneOverSz = 1.0f / TextureSz;
    inline static constexpr int TextureN = (1 << ClipmapK) - 1; // 1 row 1 col left unused
    inline static constexpr int TextureScaleHeight = 1;
    inline static constexpr int TextureScaleNormal = 1;
    inline static constexpr int TextureScaleAlbedo = 1;

    DirectX::SimpleMath::Vector2 m_GridOrigin {};    // * GridSpacing getting world offset
    DirectX::SimpleMath::Vector2 m_TexelOrigin {};    // * TextureSpacing getting texture offset
    DirectX::SimpleMath::Vector2 m_MappedOrigin    // * TextureScaleXXX getting xy offset in source texel space
    {
        static_cast<float>(std::numeric_limits<int>::min() >> 1) // prevent overflow
    };

    std::shared_ptr<DirectX::HeightMap> m_HeightSrc = nullptr;
    std::shared_ptr<DirectX::ClipmapTexture> m_HeightTex = nullptr;
    const unsigned m_Mip;
    int m_TrimPattern = 0;
};
