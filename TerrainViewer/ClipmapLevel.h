#pragma once
#include <directxtk/SimpleMath.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <filesystem>

#include "MaterialBlender.h"
#include "Texture2D.h"
#include "Vertex.h"

namespace DirectX
{
    class ClipmapTexture;
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
    ClipmapLevel(unsigned l, float gScl);
    ~ClipmapLevel() = default;

    static void BindSource(
        const std::shared_ptr<DirectX::HeightMap>& heightSrc,
        const std::shared_ptr<DirectX::SplatMap>& splatSrc,
        const std::shared_ptr<DirectX::NormalMap>& normalBase,
        const std::vector<std::shared_ptr<DirectX::AlbedoMap>>& alb,
        const std::vector<std::shared_ptr<DirectX::NormalMap>>& nor,
        const std::shared_ptr<DirectX::ClipmapTexture>& heightTex,
        const std::shared_ptr<DirectX::ClipmapTexture>& albedoTex,
        const std::shared_ptr<DirectX::ClipmapTexture>& normalTex
        );

    void UpdateOffset(
        const DirectX::SimpleMath::Vector2& view,
        const DirectX::SimpleMath::Vector2& ofsFiner);
    void UpdateTexture(ID3D11DeviceContext* context, int blendMode);
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

    static DirectX::SimpleMath::Vector2 MapToSource(const DirectX::SimpleMath::Vector2& gridOrigin)
    {
        return gridOrigin;
    }

    friend class TerrainSystem;

protected:
    using Rect = DirectX::ClipmapTexture::Rectangle;
    using HeightRect = DirectX::ClipmapTexture::UpdateArea<DirectX::HeightMap::TexelFormat>;
    using AlbedoRect = DirectX::ClipmapTexture::UpdateArea<DirectX::AlbedoMap::TexelFormat>;

    [[nodiscard]] std::vector<DirectX::HeightMap::TexelFormat> GetSourceElevation(
        int srcX, int srcY, unsigned w, unsigned h) const;

    [[nodiscard]] std::vector<DirectX::SplatMap::TexelFormat> BlendSourceAlbedo(
        int srcX, int srcY, unsigned w, unsigned h) const;

    [[nodiscard]] std::vector<DirectX::SplatMap::TexelFormat> BlendSourceNormal(
        int srcX, int srcY, unsigned w, unsigned h, int blendMode) const;

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
    inline static constexpr int TextureScaleSplat = 1;
    inline static constexpr int TextureScaleNormal = NormalBlender::SampleRatio;
    inline static constexpr int TextureScaleAlbedo = AlbedoBlender::SampleRatio;

    DirectX::SimpleMath::Vector2 m_GridOrigin {};    // * GridSpacing getting world offset
    DirectX::SimpleMath::Vector2 m_TexelOrigin {};    // * TextureSpacing getting texture offset
    DirectX::SimpleMath::Vector2 m_MappedOrigin    // * TextureScaleXXX getting xy offset in source texel space
    {
        static_cast<float>(std::numeric_limits<int>::min() >> 1) // prevent overflow
    };

    inline static std::shared_ptr<DirectX::HeightMap> m_HeightSrc = nullptr;
    inline static std::shared_ptr<DirectX::SplatMap> m_SplatSrc = nullptr;
    inline static std::shared_ptr<DirectX::NormalMap> m_NormalBase = nullptr;
    inline static std::vector<std::shared_ptr<DirectX::AlbedoMap>> m_AlbAtlas { nullptr };
    inline static std::vector<std::shared_ptr<DirectX::NormalMap>> m_NorAtlas { nullptr };
    inline static std::shared_ptr<DirectX::ClipmapTexture> m_HeightTex = nullptr;
    inline static std::shared_ptr<DirectX::ClipmapTexture> m_AlbedoTex = nullptr;
    inline static std::shared_ptr<DirectX::ClipmapTexture> m_NormalTex = nullptr;

    int m_TrimPattern = 0;
};
