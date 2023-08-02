#define NOMINMAX
#include "ClipmapLevel.h"

#include <DirectXColors.h>

#include "D3DHelper.h"
#include "Vertex.h"
#include <directxtk/BufferHelpers.h>
#include <DirectXPackedVector.h>

#include "ispc_texcomp.h"
#include "../HeightMapSplitter/ThreadPool.h"

using namespace DirectX;
using namespace SimpleMath;
using namespace PackedVector;

namespace
{
    struct Block;
    struct InteriorTrim;
    struct RingFixUp;

    template <class T>
    struct FootprintTrait;

    struct FootprintTraitBase
    {
        static constexpr int M = ClipmapM;
    };

    template <>
    struct FootprintTrait<Block> : FootprintTraitBase
    {
        // https://developer.nvidia.com/sites/all/modules/custom/gpugems/books/GPUGems2/elementLinks/02_clipmaps_05.jpg
        static constexpr Vector2 LocalOffset[] =
        {
            Vector2(0, 0),
            Vector2(M - 1, 0),
            Vector2(2 * M, 0),
            Vector2(3 * M - 1, 0),

            Vector2(0, M - 1),
            Vector2(3 * M - 1, M - 1),

            Vector2(0, 2 * M),
            Vector2(3 * M - 1, 2 * M),

            Vector2(0, 3 * M - 1),
            Vector2(M - 1, 3 * M - 1),
            Vector2(2 * M, 3 * M - 1),
            Vector2(3 * M - 1, 3 * M - 1),

            Vector2(M, M),
            Vector2(2 * M - 1, M),
            Vector2(M, 2 * M - 1),
            Vector2(2 * M - 1, 2 * M - 1),
        };

        static constexpr Vector2 Extent = Vector2 { static_cast<float>(M - 1) * 0.5f };

        inline static const XMCOLOR Color0 = XMCOLOR(Colors::DarkGreen);
        inline static const XMCOLOR Color1 = XMCOLOR(Colors::Purple);
    };

    template <>
    struct FootprintTrait<InteriorTrim> : FootprintTraitBase
    {
        //static constexpr Vector2 LocalOffset = { 0, 0 };
        static constexpr Vector2 FinerOffset[] =
        {
            Vector2(M, M),
            Vector2(M - 1, M),
            Vector2(M - 1, M - 1),
            Vector2(M, M - 1),
        };

        inline static const XMCOLOR Color = XMCOLOR(Colors::RoyalBlue);
    };

    template <>
    struct FootprintTrait<RingFixUp> : FootprintTraitBase
    {
        //static constexpr Vector2 LocalOffset = { 0, 0 };

        inline static const XMCOLOR Color = XMCOLOR(Colors::Gold);
    };
}

void ClipmapLevelBase::LoadFootprintGeometry(const std::filesystem::path& path, ID3D11Device* device)
{
    static bool loaded = false;
    if (loaded) return;

    auto loadVb = [device](const std::filesystem::path& p, auto& vb)
    {
        const auto meshVertices = LoadBinary<MeshVertex>(p);
        std::vector<GridVertex> grid;
        grid.reserve(meshVertices.size());
        std::transform(
            meshVertices.begin(),
            meshVertices.end(),
            std::back_inserter(grid),
            [](const MeshVertex& v)
            {
                return GridVertex(v.PositionX, v.PositionY);
            });

        ThrowIfFailed(CreateStaticBuffer(
            device,
            grid,
            D3D11_BIND_VERTEX_BUFFER,
            vb.GetAddressOf()));
    };

    auto loadIb = [device](const std::filesystem::path& p, auto& ib, int& idxCnt)
    {
        const auto idx = LoadBinary<std::uint16_t>(p);
        idxCnt = static_cast<int>(idx.size());
        ThrowIfFailed(CreateStaticBuffer(
            device,
            idx,
            D3D11_BIND_INDEX_BUFFER,
            ib.GetAddressOf()));
    };

    loadVb(path / "block.vtx", BlockVb);
    loadIb(path / "block.idx", BlockIb, BlockIdxCnt);
    loadVb(path / "ring_fix_up.vtx", RingFixUpVb);
    loadIb(path / "ring_fix_up.idx", RingFixUpIb, RingIdxCnt);

    loadVb(path / "trim_lt.vtx", TrimVb[0]);
    loadIb(path / "trim_lt.idx", TrimIb[0], TrimIdxCnt);
    loadVb(path / "trim_rt.vtx", TrimVb[1]);
    loadIb(path / "trim_rt.idx", TrimIb[1], TrimIdxCnt);
    loadVb(path / "trim_rb.vtx", TrimVb[2]);
    loadIb(path / "trim_rb.idx", TrimIb[2], TrimIdxCnt);
    loadVb(path / "trim_lb.vtx", TrimVb[3]);
    loadIb(path / "trim_lb.idx", TrimIb[3], TrimIdxCnt);

    loaded = true;
}


ClipmapLevel::ClipmapLevel(const unsigned l, const float gScl, Vector3 view) :
    m_Level(l), m_GridSpacing(gScl) {}

void ClipmapLevel::BindSourceManager(
    const std::shared_ptr<BitmapManager>& srcMgr)
{
    s_SrcManager = srcMgr;
}

void ClipmapLevel::BindTexture(
    const std::shared_ptr<ClipmapTexture>& height,
    const std::shared_ptr<ClipmapTexture>& albedo,
    const std::shared_ptr<ClipmapTexture>& normal)
{
    s_HeightTex = height;
    s_AlbedoTex = albedo;
    s_NormalTex = normal;
}

void ClipmapLevel::TickTransform(
    const Vector3& view, int& budget, const float hScale, const int blendMode)
{
    //  - - - - - - - -
    //  |             | 
    //  |   O         | dx
    //  |     - - - - - - - -
    //  |     |///////|     |
    //  |     |///////|  I  |   dh - abs(dy)
    //  |     |///////|     |
    //  - - - |- - - - - - -|
    //        |             |   abs(dy)
    //        |     I       |   
    //        - - - - - - - -
    //              dw

    const auto prev = m_GridOrigin;
    const Vector2 curr(
        std::ceil(view.x / m_GridSpacing * 0.5f) * 2 - 128,
        std::ceil(view.z / m_GridSpacing * 0.5f) * 2 - 128);

    const int dx = static_cast<int>(curr.x - prev.x);
    const int dy = static_cast<int>(curr.y - prev.y);
    const bool updateWhole = std::abs(dx) >= TextureN || std::abs(dy) >= TextureN;
    const int cost = updateWhole
                         ? TextureN * TextureN   // update whole tex anyway
                         : TextureN * std::abs(dy) + std::abs(dx) * (TextureN - std::abs(dy));

    const bool insufficient = budget < cost;
    const auto height = s_SrcManager->GetPixelHeight(m_GridOrigin.x + 128, m_GridOrigin.y + 128, m_Level);
    const bool farAbove = std::abs(view.y - hScale * height) > 2.5f * 254.0f * m_GridSpacing;
    if (insufficient || farAbove)
    {
        m_IsActive = false;
        return;
    }

    budget -= cost;
    m_IsActive = true; // update texture before render tick
    m_GridOrigin = curr;

    // GenerateTextureAsync
    auto generateHeight = [this](const Rect rect, const int srcX, const int srcY)
    {
        return ClipmapTexture::UpdateArea(rect, m_Level,
            s_SrcManager->CopyElevation(srcX, srcY, rect.W, rect.H, m_Level));
    };

    auto generateAlbedo = [this](const Rect rect, const int srcX, const int srcY)
    {
        const auto src =
#ifdef HARDWARE_FILTERING
            s_SrcManager->BlendAlbedoRoughness(srcX, srcY, rect.W, rect.H, m_Level);
#else
            s_SrcManager->BlendAlbedoRoughnessBc3(srcX, srcY, rect.W, rect.H, m_Level);
#endif
        return ClipmapTexture::UpdateArea(rect * TextureScaleAlbedo, m_Level, src);
    };

    auto generateNormal = [this, blendMode](const Rect rect, const int srcX, const int srcY)
    {
        const auto src =
#ifdef HARDWARE_FILTERING
            s_SrcManager->BlendNormalOcclusion(srcX, srcY, rect.W, rect.H, m_Level, blendMode);
#else
            s_SrcManager->BlendNormalOcclusionBc3(srcX, srcY, rect.W, rect.H, m_Level, blendMode);
#endif
        return ClipmapTexture::UpdateArea(rect * TextureScaleNormal, m_Level, src);
    };

    auto generateAsync = [generateNormal, generateAlbedo, generateHeight]
    (const Rect rect, const int srcX, const int srcY)
    {
        if (rect.IsEmpty()) return;
        s_NormalStream.emplace_back(g_ThreadPool.enqueue(generateNormal, rect, srcX, srcY));
        s_AlbedoStream.emplace_back(g_ThreadPool.enqueue(generateAlbedo, rect, srcX, srcY));
        s_HeightStream.emplace_back(g_ThreadPool.enqueue(generateHeight, rect, srcX, srcY));
    };

    if (updateWhole)
    {
        const Rect dwdh(0, 0, TextureN, TextureN);
        generateAsync(dwdh, curr.x, curr.y);
        m_TexelOrigin = Vector2::Zero;
        return;
    }

    // update dy * dw first for cache coherence
    const Rect dwdy(
        WarpMod(static_cast<int>(m_TexelOrigin.x) + dx, TextureSz),
        WarpMod(static_cast<int>(m_TexelOrigin.y) + (dy >= 0 ? TextureN : dy), TextureSz),
        TextureN, std::abs(dy));
    generateAsync(dwdy, curr.x, dy >= 0 ? prev.y + TextureN : curr.y);

    // then update dx * (dh - abs(dy))
    const Rect dxdh(
        WarpMod(static_cast<int>(m_TexelOrigin.x) + (dx >= 0 ? TextureN : dx), TextureSz),
        WarpMod(static_cast<int>(m_TexelOrigin.y) + (dy >= 0 ? dy : 0), TextureSz),
        std::abs(dx), TextureN - std::abs(dy));
    generateAsync(dxdh, dx >= 0 ? prev.x + TextureN : curr.x, dy >= 0 ? curr.y : prev.y);

    m_TexelOrigin.x = WarpMod(dx + static_cast<int>(m_TexelOrigin.x), TextureSz);
    m_TexelOrigin.y = WarpMod(dy + static_cast<int>(m_TexelOrigin.y), TextureSz);
}

void ClipmapLevel::UpdateTexture(ID3D11DeviceContext* context)
{
    if (s_HeightStream.empty()) return;

    // can't use Map method for resource that arraySlice > 0
    // const MapGuard hm(context, s_HeightTex->GetTexture(),
    //     D3D11CalcSubresource(0, m_Lod, 1), D3D11_MAP_WRITE, 0);

    const auto begin = std::chrono::steady_clock::now();
    for (auto&& future : s_HeightStream)
        s_HeightTex->UpdateToroidal(context, future.get());
    for (auto&& future : s_AlbedoStream)
        s_AlbedoTex->UpdateToroidal(context, future.get());
    for (auto&& future : s_NormalStream)
        s_NormalTex->UpdateToroidal(context, future.get());
    const auto end = std::chrono::steady_clock::now();
    // std::printf("UpdateTexture: %f ms\n",
    //     std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1000.0f);

    s_HeightStream.clear();
    s_AlbedoStream.clear();
    s_NormalStream.clear();

#ifdef HARDWARE_FILTERING
    s_AlbedoTex->GenerateMips(context);
    s_NormalTex->GenerateMips(context);
#endif
}

ClipmapLevelBase::HollowRing ClipmapLevel::GetHollowRing(
    const Vector2& textureOriginCoarse, const Vector2& worldOriginFiner,
    const BoundingFrustum& frustum, float hScl, bool top) const
{
    using namespace SimpleMath;

    HollowRing hollow;
    hollow.TrimId = GetTrimPattern(worldOriginFiner);

    const GridInstance ins
    {
        m_GridSpacing,
        top ? 1.0f / TextureSz : 0.5f / TextureSz,
        Vector2(m_GridSpacing) * m_GridOrigin,
        (Vector2(m_TexelOrigin.x, m_TexelOrigin.y)) * TextureSzRcp,
        textureOriginCoarse,
        XMUINT2(),
        0,
        m_Level
    };

    for (int i = 0; i < 12; ++i)
    {
        GridInstance block = ins;
        using Trait = FootprintTrait<Block>;
        block.WorldOffset += Trait::LocalOffset[i] * m_GridSpacing;
        if (!IsBlockVisible(block.WorldOffset, frustum, hScl)) continue;

        block.TextureOffsetFiner += Trait::LocalOffset[i] * TextureSzRcp;
        block.TextureOffsetCoarser += Trait::LocalOffset[i] * block.CoarserStepRate;
        block.LocalOffset = XMUINT2(Trait::LocalOffset[i].x, Trait::LocalOffset[i].y);
        block.Color = i == 0 || i == 2 || i == 5 || i == 11 || i == 6 || i == 9
                          ? Trait::Color0
                          : Trait::Color1;
        hollow.Blocks.emplace_back(std::move(block));
    }

    {
        GridInstance& fixUp = hollow.RingFixUp;
        fixUp = ins;
        fixUp.Color = FootprintTrait<RingFixUp>::Color;
    }

    {
        GridInstance& trim = hollow.Trim;
        trim = ins;
        trim.Color = FootprintTrait<InteriorTrim>::Color;
    }

    return std::move(hollow);
}


ClipmapLevelBase::SolidSquare ClipmapLevel::GetSolidSquare(
    const Vector2& textureOriginCoarse, const BoundingFrustum& frustum, float hScl, bool top) const
{
    using namespace SimpleMath;
    SolidSquare solid;

    const GridInstance ins
    {
        m_GridSpacing,
        top ? 1.0f / TextureSz : 0.5f / TextureSz,
        (Vector2(m_GridSpacing) * m_GridOrigin),
        Vector2(m_TexelOrigin.x, m_TexelOrigin.y) * TextureSzRcp,
        textureOriginCoarse,
        XMUINT2(),
        0,
        m_Level
    };

    for (int i = 0; i < 16; ++i)
    {
        GridInstance block = ins;
        using Trait = FootprintTrait<Block>;
        block.WorldOffset += Trait::LocalOffset[i] * m_GridSpacing;
        if (!IsBlockVisible(block.WorldOffset, frustum, hScl)) continue;

        block.TextureOffsetFiner += Trait::LocalOffset[i] * TextureSzRcp;
        block.TextureOffsetCoarser += Trait::LocalOffset[i] * block.CoarserStepRate;
        block.LocalOffset = XMUINT2(Trait::LocalOffset[i].x, Trait::LocalOffset[i].y);
        block.Color = i == 0 || i == 2 || i == 5 || i == 11 || i == 6 || i == 9 || i == 12 || i == 15
                          ? Trait::Color0
                          : Trait::Color1;
        solid.Blocks.emplace_back(std::move(block));
    }

    {
        GridInstance& fixUp = solid.RingFixUp;
        fixUp = ins;
        fixUp.Color = FootprintTrait<RingFixUp>::Color;
    }

    for (auto& trim : solid.Trim)
    {
        trim = ins;
        trim.Color = FootprintTrait<InteriorTrim>::Color;
    }

    return std::move(solid);
}

Vector2 ClipmapLevel::GetFinerUvOffset(const Vector2& finerWld) const
{
    return (m_TexelOrigin + FootprintTrait<InteriorTrim>::FinerOffset[GetTrimPattern(finerWld)]) * TextureSzRcp;
}

bool ClipmapLevel::IsBlockVisible(const Vector2& world, const BoundingFrustum& frustum, float scl) const
{
    using Trait = FootprintTrait<Block>;
    constexpr float min = 0.1;
    constexpr float max = 0.5;
    constexpr float avg = (min + max) * 0.5f;
    constexpr float diff = max - min;
    const auto center = Vector3(
        world.x + Trait::Extent.x * m_GridSpacing,
        avg * scl,
        world.y + Trait::Extent.y * m_GridSpacing);
    const auto extents = Vector3(
        Trait::Extent.x * m_GridSpacing,
        diff * 0.5f * scl,
        Trait::Extent.y * m_GridSpacing);
    return frustum.Contains(BoundingBox(center, extents)) != DISJOINT;
}
