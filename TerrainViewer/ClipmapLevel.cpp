#define NOMINMAX
#include "ClipmapLevel.h"

#include <DirectXColors.h>

#include "D3DHelper.h"
#include "Vertex.h"
#include <directxtk/BufferHelpers.h>
#include <DirectXPackedVector.h>

#include "BitMap.h"
#include "MaterialBlender.h"
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


ClipmapLevel::ClipmapLevel(const unsigned l, const float gScl, DirectX::SimpleMath::Vector3 view) :
    m_Level(l), m_GridSpacing(gScl)
{
    m_GridOrigin.x = std::ceil(view.x / m_GridSpacing * 0.5f) * 2 - 128;
    m_GridOrigin.y = std::ceil(view.z / m_GridSpacing * 0.5f) * 2 - 128;
}

void ClipmapLevel::BindSource(
    const std::shared_ptr<HeightMap>& heightSrc, const std::shared_ptr<SplatMap>& splatSrc,
    const std::shared_ptr<NormalMap>& normalBase, const std::vector<std::shared_ptr<AlbedoMap>>& alb,
    const std::vector<std::shared_ptr<NormalMap>>& nor,
    const std::shared_ptr<ClipmapTexture>& heightTex,
    const std::shared_ptr<ClipmapTexture>& albedoTex,
    const std::shared_ptr<ClipmapTexture>& normalTex)
{
    m_HeightSrc = heightSrc;
    m_SplatSrc = splatSrc;
    m_NormalBase = normalBase;
    m_AlbAtlas = alb;
    m_NorAtlas = nor;
    m_HeightTex = heightTex;
    m_AlbedoTex = albedoTex;
    m_NormalTex = normalTex;
}

void ClipmapLevel::UpdateTransform(
    const Vector3& view, const Vector3& speed,
    const int blendMode, const float hScale, int& budget)
{
    m_GridOrigin.x = std::ceil(view.x / m_GridSpacing * 0.5f) * 2 - 128;
    m_GridOrigin.y = std::ceil(view.z / m_GridSpacing * 0.5f) * 2 - 128;

    const int dx = static_cast<int>(m_GridOrigin.x - m_MappedOrigin.x);
    const int dy = static_cast<int>(m_GridOrigin.y - m_MappedOrigin.y);
    const int cost = std::min(
        TextureN * std::abs(dy) + std::abs(dx) * (TextureN - std::abs(dy)),
        TextureN * TextureN);

    if (budget < cost || std::abs(view.y - hScale * GetHeight()) > 2.5f * 254.0f * m_GridSpacing)
    {
        m_IsActive = 0;
        return;
    }
    budget -= cost;
    if (m_IsActive == 0 || m_IsActive == 1) m_IsActive++;

    // GenerateTextureAsync
    auto generateHeight = [this](const Rect rect, const int srcX, const int srcY)
    {
        return ClipmapTexture::UpdateArea(rect * TextureScaleHeight, m_Level,
            GetElevation(srcX, srcY, rect.W * TextureScaleHeight, rect.H * TextureScaleHeight));
    };

    auto generateAlbedo = [this](const Rect rect, const int srcX, const int srcY)
    {
        return ClipmapTexture::UpdateArea(rect * TextureScaleAlbedo, m_Level,
            CompressRgba8ToBc3(BlendAlbedoRoughness(srcX, srcY, rect.W, rect.H).data(),
                rect.W * TextureScaleAlbedo, rect.H * TextureScaleAlbedo));
    };

    auto generateNormal = [this, blendMode](const Rect rect, const int srcX, const int srcY)
    {
        return ClipmapTexture::UpdateArea(rect * TextureScaleNormal, m_Level,
            CompressRgba8ToBc3(BlendNormalOcclusion(srcX, srcY, rect.W, rect.H, blendMode).data(),
                rect.W * TextureScaleNormal, rect.H * TextureScaleNormal));
    };

    auto generateAsync = [generateNormal, generateAlbedo, generateHeight]
    (const Rect rect, const int srcX, const int srcY)
    {
        if (rect.IsEmpty()) return;
        m_NormalStream.emplace_back(g_ThreadPool.enqueue(generateNormal, rect, srcX, srcY));
        m_AlbedoStream.emplace_back(g_ThreadPool.enqueue(generateAlbedo, rect, srcX, srcY));
        m_HeightStream.emplace_back(g_ThreadPool.enqueue(generateHeight, rect, srcX, srcY));
    };

    if (std::abs(dx) >= TextureN || std::abs(dy) >= TextureN) // update full tex anyway
    {
        m_TexelOrigin = Vector2::Zero;
        const Rect dwdh(0, 0, TextureN, TextureN);
        generateAsync(dwdh, m_GridOrigin.x, m_GridOrigin.y);
    }
    else
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

        // update dy * dw first for cache coherence
        const Rect dwdy(
            WarpMod(static_cast<int>(m_TexelOrigin.x) + dx, TextureSz),
            WarpMod(static_cast<int>(m_TexelOrigin.y) + (dy >= 0 ? TextureN : dy), TextureSz),
            TextureN, std::abs(dy));
        generateAsync(dwdy, m_GridOrigin.x, dy >= 0 ? m_MappedOrigin.y + TextureN : m_GridOrigin.y);

        // then update dx * (dh - abs(dy))
        const Rect dxdh(
            WarpMod(static_cast<int>(m_TexelOrigin.x) + (dx >= 0 ? TextureN : dx), TextureSz),
            WarpMod(static_cast<int>(m_TexelOrigin.y) + (dy >= 0 ? dy : 0), TextureSz),
            std::abs(dx), TextureN - std::abs(dy));
        generateAsync(dxdh, dx >= 0 ? m_MappedOrigin.x + TextureN : m_GridOrigin.x,
            dy >= 0 ? m_GridOrigin.y : m_MappedOrigin.y);

        m_TexelOrigin.x = WarpMod(dx + static_cast<int>(m_TexelOrigin.x), TextureSz);
        m_TexelOrigin.y = WarpMod(dy + static_cast<int>(m_TexelOrigin.y), TextureSz);
    }
    m_MappedOrigin = m_GridOrigin;
}

void ClipmapLevel::UpdateTexture(ID3D11DeviceContext* context)
{
    if (m_HeightStream.empty()) return;

    // can't use Map method for resource that arraySlice > 0
    // const MapGuard hm(context, m_HeightTex->GetTexture(),
    //     D3D11CalcSubresource(0, m_Lod, 1), D3D11_MAP_WRITE, 0);
    
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    for (auto&& future : m_HeightStream)
        m_HeightTex->UpdateToroidal(context, future.get());
    for (auto&& future : m_AlbedoStream)
        m_AlbedoTex->UpdateToroidal(context, future.get());
    for (auto&& future : m_NormalStream)
        m_NormalTex->UpdateToroidal(context, future.get());
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    // std::printf("UpdateTexture: %f ms\n",
    //     std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1000.0f);

    m_HeightStream.clear();
    m_AlbedoStream.clear();
    m_NormalStream.clear();

#ifdef HARDWARE_FILTERING
    m_AlbedoCm->GenerateMips(context);
    m_NormalCm->GenerateMips(context);
#endif
}

ClipmapLevelBase::HollowRing ClipmapLevel::GetHollowRing(
    const Vector2& textureOriginCoarse, const Vector2& worldOriginFiner) const
{
    using namespace SimpleMath;

    HollowRing hollow;
    hollow.TrimId = GetTrimPattern(worldOriginFiner);

    const GridInstance ins
    {
        Vector2(m_GridSpacing),
        Vector2(m_GridSpacing) * m_GridOrigin,
        (Vector2(m_TexelOrigin.x, m_TexelOrigin.y)) * OneOverSz,
        textureOriginCoarse,
        XMUINT2(),
        0,
        m_Level
    };

    for (int i = 0; i < 12; ++i)
    {
        GridInstance& block = hollow.Blocks[i];
        using Trait = FootprintTrait<Block>;
        block = ins;
        block.WorldOffset += Trait::LocalOffset[i] * m_GridSpacing;
        block.TextureOffsetFiner += Trait::LocalOffset[i] * OneOverSz;
        block.TextureOffsetCoarser += Trait::LocalOffset[i] * (OneOverSz * 0.5f);
        block.LocalOffset = XMUINT2(Trait::LocalOffset[i].x, Trait::LocalOffset[i].y);
        block.Color = i == 0 || i == 2 || i == 5 || i == 11 || i == 6 || i == 9
                          ? Trait::Color0
                          : Trait::Color1;
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


ClipmapLevelBase::SolidSquare ClipmapLevel::GetSolidSquare(const Vector2& textureOriginCoarse) const
{
    using namespace SimpleMath;
    SolidSquare solid;

    const GridInstance ins
    {
        Vector2(m_GridSpacing),
        (Vector2(m_GridSpacing) * m_GridOrigin),
        Vector2(m_TexelOrigin.x, m_TexelOrigin.y) * OneOverSz,
        textureOriginCoarse,
        XMUINT2(),
        0,
        m_Level
    };

    for (int i = 0; i < 16; ++i)
    {
        GridInstance& block = solid.Blocks[i];
        using Trait = FootprintTrait<Block>;
        block = ins;
        block.WorldOffset += Trait::LocalOffset[i] * m_GridSpacing;
        block.TextureOffsetFiner += Trait::LocalOffset[i] * OneOverSz;
        block.TextureOffsetCoarser += Trait::LocalOffset[i] * (OneOverSz * 0.5f);
        block.LocalOffset = XMUINT2(Trait::LocalOffset[i].x, Trait::LocalOffset[i].y);
        block.Color = i == 0 || i == 2 || i == 5 || i == 11 || i == 6 || i == 9 || i == 12 || i == 15
                          ? Trait::Color0
                          : Trait::Color1;
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

float ClipmapLevel::GetHeight() const
{
    return m_HeightSrc->GetPixelHeight(m_GridOrigin.x + 128, m_GridOrigin.y + 128, 0);
}

Vector2 ClipmapLevel::GetFinerTextureOffset(const Vector2& finer) const
{
    return (m_TexelOrigin + FootprintTrait<InteriorTrim>::FinerOffset[GetTrimPattern(finer)]) * OneOverSz;
}

std::vector<HeightMap::TexelFormat> ClipmapLevel::GetElevation(
    const int srcX, const int srcY, const unsigned w, const unsigned h) const
{
    return m_HeightSrc->CopyRectangle(
        srcX * TextureScaleHeight, srcY * TextureScaleHeight,
        w * TextureScaleHeight, h * TextureScaleHeight, m_Level);
}

std::vector<AlbedoMap::TexelFormat> ClipmapLevel::BlendAlbedoRoughness(
    const int srcX, const int srcY, const unsigned w, const unsigned h) const
{
    return AlbedoBlender(m_SplatSrc, m_AlbAtlas).Blend(
        srcX * TextureScaleSplat, srcY * TextureScaleSplat,
        w * TextureScaleSplat, h * TextureScaleSplat, m_Level);
}

std::vector<NormalMap::TexelFormat> ClipmapLevel::BlendNormalOcclusion(
    const int srcX, const int srcY, const unsigned w, const unsigned h, int blendMode) const
{
    return NormalBlender(m_SplatSrc, m_NormalBase, m_NorAtlas).Blend(
        srcX * TextureScaleSplat, srcY * TextureScaleSplat,
        w * TextureScaleSplat, h * TextureScaleSplat, m_Level,
        static_cast<NormalBlender::BlendMethod>(blendMode));
}

std::vector<uint8_t> ClipmapLevel::CompressRgba8ToBc3(
    uint32_t* src, const unsigned w, const unsigned h)
{
    // 16 bytes per 4x4 pixel <=> 1 bytes per pixel
    std::vector<std::uint8_t> dst(w * h);
    rgba_surface surface;
    surface.width = w;
    surface.height = h;
    surface.stride = w * sizeof(uint32_t);
    surface.ptr = reinterpret_cast<uint8_t*>(src);
    CompressBlocksBC3(&surface, dst.data());
    return dst;
}

std::vector<uint8_t> ClipmapLevel::CompressRgba8ToBc7(uint32_t* src, const unsigned w, const unsigned h)
{
    std::vector<std::uint8_t> dst(w * h);
    rgba_surface surface;
    surface.width = w;
    surface.height = h;
    surface.stride = w * sizeof(uint32_t);
    surface.ptr = reinterpret_cast<uint8_t*>(src);
    bc7_enc_settings settings;
    GetProfile_ultrafast(&settings);
    CompressBlocksBC7(&surface, dst.data(), &settings);
    return dst;
}
