#pragma once

#include <directxtk/SimpleMath.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <filesystem>
#include <future>

#include "BitmapManager.h"
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
        std::vector<GridInstance> Blocks;
        GridInstance RingFixUp;
        GridInstance Trim;
        int TrimId;
    };

    struct SolidSquare
    {
        std::vector<GridInstance> Blocks;
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

    static void BindSourceManager(const std::shared_ptr<BitmapManager>& srcMgr);
    static void BindTexture(
        const std::shared_ptr<DirectX::ClipmapTexture>& height,
        const std::shared_ptr<DirectX::ClipmapTexture>& albedo,
        const std::shared_ptr<DirectX::ClipmapTexture>& normal);

    void TickTransform(const DirectX::SimpleMath::Vector3& view, int& budget, float hScale, int blendMode);
    static void UpdateTexture(ID3D11DeviceContext* context);

    [[nodiscard]] HollowRing GetHollowRing(
        const DirectX::SimpleMath::Vector2& textureOriginCoarse,
        const DirectX::SimpleMath::Vector2& worldOriginFiner,
        const DirectX::BoundingFrustum& frustum, float hScl, bool top) const;
    [[nodiscard]] SolidSquare GetSolidSquare(
        const DirectX::SimpleMath::Vector2& textureOriginCoarse,
        const DirectX::BoundingFrustum& frustum, float hScl, bool top) const;

    [[nodiscard]] DirectX::SimpleMath::Vector2 GetWorldOffset() const { return m_GridOrigin * m_GridSpacing; }
    [[nodiscard]] DirectX::SimpleMath::Vector2 GetFinerUvOffset(const DirectX::SimpleMath::Vector2& finer) const;
    [[nodiscard]] DirectX::SimpleMath::Vector2 GetUvOffset() const { return m_TexelOrigin * TextureSzRcp; }
    [[nodiscard]] bool IsActive() const { return m_IsActive; }

    friend class TerrainSystem;

protected:
    using Rect = DirectX::ClipmapTexture::Rectangle;
    using HeightRect = DirectX::ClipmapTexture::UpdateArea<DirectX::HeightMap::PixelFormat>;
    using AlbedoRect = DirectX::ClipmapTexture::UpdateArea<DirectX::AlbedoMap::PixelFormat>;

    inline static constexpr int TextureSz = 1 << ClipmapK;
    inline static constexpr float TextureSzRcp = 1.0f / TextureSz;
    inline static constexpr int TextureN = (1 << ClipmapK) - 1; // 1 row 1 col left unused
    inline static constexpr int TextureScaleHeight = 1;
    inline static constexpr int TextureScaleSplat = 1;
    inline static constexpr int TextureScaleNormal = NormalBlender::SampleRatio;
    inline static constexpr int TextureScaleAlbedo = AlbedoBlender::SampleRatio;

    inline static constexpr DXGI_FORMAT HeightFormat = DXGI_FORMAT_R16_UNORM;
#ifdef HARDWARE_FILTERING
    inline static constexpr DXGI_FORMAT AlbedoFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    inline static constexpr DXGI_FORMAT NormalFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
#else
    inline static constexpr DXGI_FORMAT AlbedoFormat = DXGI_FORMAT_BC3_UNORM;
    inline static constexpr DXGI_FORMAT NormalFormat = DXGI_FORMAT_BC3_UNORM;
#endif

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

    [[nodiscard]] bool IsBlockVisible(
        const DirectX::SimpleMath::Vector2& world, const DirectX::BoundingFrustum& frustum, float scl) const;

    const unsigned m_Level;
    const float m_GridSpacing;
    DirectX::SimpleMath::Vector2 m_GridOrigin           // * GridSpacing getting world offset
    {
        static_cast<float>(std::numeric_limits<int>::min() >> 1) // prevent overflow
    };
    DirectX::SimpleMath::Vector2 m_TexelOrigin {};      // * TextureSpacing getting texture offset

    using ResultR16 = std::future<DirectX::ClipmapTexture::UpdateArea<uint16_t>>;
    using ResultBc = std::future<DirectX::ClipmapTexture::UpdateArea<uint8_t>>;
    using ResultRgba8888 = std::future<DirectX::ClipmapTexture::UpdateArea<uint32_t>>;

    inline static std::shared_ptr<BitmapManager> s_SrcManager {};
    inline static std::shared_ptr<DirectX::ClipmapTexture> s_HeightTex = nullptr;
    inline static std::shared_ptr<DirectX::ClipmapTexture> s_AlbedoTex = nullptr;
    inline static std::shared_ptr<DirectX::ClipmapTexture> s_NormalTex = nullptr;
    inline static std::vector<ResultR16> s_HeightStream {};
#ifdef  HARDWARE_FILTERING
    inline static std::vector<ResultRgba8888> s_AlbedoStream {};
    inline static std::vector<ResultRgba8888> s_NormalStream {};
#else
    inline static std::vector<ResultBc> s_AlbedoStream {};
    inline static std::vector<ResultBc> s_NormalStream {};
#endif

    int m_IsActive = 0; // de-active / updating / active
};
