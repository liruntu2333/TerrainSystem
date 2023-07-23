#pragma once

#include <directxtk/SimpleMath.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <filesystem>
#include <future>

#include "MaterialBlender.h"
#include "Texture2D.h"
#include "Vertex.h"

static constexpr int ClipmapK = 8;
static constexpr int ClipmapN = (1 << ClipmapK) - 1;
static constexpr int ClipmapM = (ClipmapN + 1) / 4;

struct rgba_surface;

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
    ClipmapLevel() = delete;
    ClipmapLevel(unsigned l, float gScl, DirectX::SimpleMath::Vector3 view);
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

    void UpdateTransform(
        const DirectX::SimpleMath::Vector3& view,
        const DirectX::SimpleMath::Vector3& speed,
        int blendMode, float hScale, int& budget);
    static void UpdateTexture(ID3D11DeviceContext* context);

    [[nodiscard]] HollowRing GetHollowRing(
        const DirectX::SimpleMath::Vector2& textureOriginCoarse,
        const DirectX::SimpleMath::Vector2& worldOriginFiner) const;
    [[nodiscard]] SolidSquare GetSolidSquare(
        const DirectX::SimpleMath::Vector2& textureOriginCoarse) const;
    [[nodiscard]] float GetHeight() const;
    [[nodiscard]] DirectX::SimpleMath::Vector2 GetFinerTextureOffset(
        const DirectX::SimpleMath::Vector2& finer) const;

    [[nodiscard]] bool IsActive() const
    {
        return m_IsActive == 2;
    }

    friend class TerrainSystem;

protected:
    using Rect = DirectX::ClipmapTexture::Rectangle;
    using HeightRect = DirectX::ClipmapTexture::UpdateArea<DirectX::HeightMap::TexelFormat>;
    using AlbedoRect = DirectX::ClipmapTexture::UpdateArea<DirectX::AlbedoMap::TexelFormat>;

    [[nodiscard]] std::vector<DirectX::HeightMap::TexelFormat> GetElevation(
        int srcX, int srcY, unsigned w, unsigned h) const;
    [[nodiscard]] std::vector<DirectX::AlbedoMap::TexelFormat> BlendAlbedoRoughness(
        int srcX, int srcY, unsigned w, unsigned h) const;
    [[nodiscard]] std::vector<DirectX::NormalMap::TexelFormat> BlendNormalOcclusion(
        int srcX, int srcY, unsigned w, unsigned h, int blendMode) const;

    static [[nodiscard]] std::vector<uint8_t> CompressRgba8ToBc3(
        uint32_t* src, unsigned w, unsigned h);
    static [[nodiscard]] std::vector<uint8_t> CompressRgba8ToBc7(
        uint32_t* src, unsigned w, unsigned h);

    [[nodiscard]] DirectX::SimpleMath::Vector2 GetWorldOffset() const
    {
        return m_GridOrigin * m_GridSpacing;
    }

    static DirectX::SimpleMath::Vector2 MapToSource(const DirectX::SimpleMath::Vector2& gridOrigin)
    {
        return gridOrigin;
    }

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

    inline static constexpr DXGI_FORMAT HeightFormat = DXGI_FORMAT_R16_UNORM;
    inline static constexpr DXGI_FORMAT AlbedoFormat = DXGI_FORMAT_BC3_UNORM;
    inline static constexpr DXGI_FORMAT NormalFormat = DXGI_FORMAT_BC3_UNORM;

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

    using ResultR16 = std::future<DirectX::ClipmapTexture::UpdateArea<uint16_t>>;
    using ResultBC = std::future<DirectX::ClipmapTexture::UpdateArea<uint8_t>>;
    inline static std::vector<ResultR16> m_HeightStream {};
    inline static std::vector<ResultBC> m_AlbedoStream {};
    inline static std::vector<ResultBC> m_NormalStream {};

    int m_IsActive = 0; // de-active / updating / active
};
