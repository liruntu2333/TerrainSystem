#define NOMINMAX
#include "ClipmapLevel.h"

#include <DirectXColors.h>

#include "D3DHelper.h"
#include "Vertex.h"
#include <directxtk/BufferHelpers.h>
#include <DirectXPackedVector.h>
#include <queue>

#include "HeightMap.h"
#include "Texture2D.h"

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

    template <class T>
    std::vector<T> CopyWarp(
        const T* src, int sw, int sh,
        int dx, int dy, int sx, int sy)
    {
        assert(dx >= 0 && dy >= 0 && "should update from lt to rb");
        std::vector<T> rect(dx * dy);

        for (int y = 0; y < dy; ++y)
            for (int x = 0; x < dx; ++x)
                rect[y * dx + x] = src[WarpMod(sy + y, sh) * sw + WarpMod(sx + x, sw)];

        return rect;
    }
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


ClipmapLevel::ClipmapLevel(
    unsigned l, float gScl, const Vector2& view,
    const std::shared_ptr<HeightMap>& src, const std::shared_ptr<ClipmapTexture>& hTex) :
    m_Level(l), m_GridSpacing(gScl),
    m_HeightSrc(src), m_HeightTex(hTex), m_Subresource(D3D11CalcSubresource(0, l, 1)) {}

void ClipmapLevel::UpdateOffset(const Vector2& dView, const Vector2& view, const Vector2& ofsFiner)
{
    m_GridOrigin.x = ceil(view.x / m_GridSpacing * 0.5f) * 2 - 128;
    m_GridOrigin.y = ceil(view.y / m_GridSpacing * 0.5f) * 2 - 128;
    m_TrimPattern = GetTrimPattern(ofsFiner);
}

void ClipmapLevel::UpdateTexture(ID3D11DeviceContext* context)
{
    const int dx = m_GridOrigin.x - m_MappedOrigin.x;
    const int dy = m_GridOrigin.y - m_MappedOrigin.y;
    if (dx == 0 && dy == 0) return;

    // can't use Map method for resource that arraySlice > 0
    // const MapGuard hm(context, m_HeightTex->GetTexture(),
    //     D3D11CalcSubresource(0, m_Subresource, 1), D3D11_MAP_WRITE, 0);

    const uint16_t* src = m_HeightSrc->GetData();
    const int sw = m_HeightSrc->GetWidth();
    const int sh = m_HeightSrc->GetHeight();
    if (std::abs(dx) >= TextureN || std::abs(dy) >= TextureN) // update full tex anyway
    {
        m_TexelOrigin = Vector2::Zero;
        const auto rect = CopyWarp(src, sw, sh, TextureN, TextureN, m_GridOrigin.x, m_GridOrigin.y);
        m_MappedOrigin = m_GridOrigin;
        context->UpdateSubresource(m_HeightTex->GetTexture(), m_Subresource,
            nullptr, rect.data(), TextureN * sizeof uint16_t, 0);
        return;
    }

    struct UpdateArea
    {
        unsigned X {}, Y {}, W {}, H {};
        std::vector<uint16_t> Source {};
    };

    // update dy * dw first for cache coherence
    UpdateArea dwdy;
    dwdy.W = TextureN;
    dwdy.H = std::abs(dy);
    dwdy.X = WarpMod(static_cast<int>(m_TexelOrigin.x) + dx, TextureSz);
    dwdy.Y = WarpMod(static_cast<int>(m_TexelOrigin.y) + (dy >= 0 ? TextureN : dy), TextureSz);
    dwdy.Source = CopyWarp(src, sw, sh,
        TextureN, std::abs(dy),
        static_cast<int>(m_MappedOrigin.x) + dx,
        static_cast<int>(m_MappedOrigin.y) + (dy >= 0 ? TextureN : dy));

    // then update dx * (dh - abs(dy))
    UpdateArea dxdh;
    dxdh.W = std::abs(dx);
    dxdh.H = TextureN - std::abs(dy);
    dxdh.X = WarpMod(static_cast<int>(m_TexelOrigin.x) + (dx >= 0 ? TextureN : dx), TextureSz);
    dxdh.Y = WarpMod(static_cast<int>(m_TexelOrigin.y) + (dy >= 0 ? dy : 0), TextureSz);
    dxdh.Source = CopyWarp(src, sw, sh,
        std::abs(dx), TextureN - std::abs(dy),
        static_cast<int>(m_MappedOrigin.x) + (dx >= 0 ? TextureN : dx),
        static_cast<int>(m_MappedOrigin.y) + (dy >= 0 ? dy : 0));

    std::queue<UpdateArea> q;
    if (dwdy.W > 0 && dwdy.H > 0) q.push(dwdy);
    if (dxdh.W > 0 && dxdh.H > 0) q.push(dxdh);
    while (!q.empty())
    {
        auto& [x, y, w, h, s] = q.front();

        if (x + w > TextureSz)
        {
            const unsigned w1 = TextureSz - x;
            const unsigned w2 = w - w1;
            std::vector<uint16_t> src1(w1 * h);
            std::vector<uint16_t> src2(w2 * h);
            for (unsigned i = 0; i < h; ++i)
            {
                std::copy_n(s.begin() + i * w, w1, src1.begin() + i * w1);
                std::copy_n(s.begin() + i * w + w1, w2, src2.begin() + i * w2);
            }
            q.push({ x, y, w1, h, std::move(src1) });
            q.push({ 0, y, w2, h, std::move(src2) });
        }
        else if (y + h > TextureSz)
        {
            const unsigned h1 = TextureSz - y;
            const unsigned h2 = h - h1;
            std::vector<uint16_t> src1(w * h1);
            std::vector<uint16_t> src2(w * h2);
            std::copy_n(s.begin(), w * h1, src1.begin());
            std::copy_n(s.begin() + w * h1, w * h2, src2.begin());
            q.push({ x, y, w, h1, std::move(src1) });
            q.push({ x, 0, w, h2, std::move(src2) });
        }
        else
        {
            CD3D11_BOX box(x, y, 0, x + w, y + h, 1);
            context->UpdateSubresource(m_HeightTex->GetTexture(), m_Subresource,
                &box, s.data(), w * sizeof uint16_t, 0);
        }
        q.pop();
    }

    m_MappedOrigin = m_GridOrigin;
    m_TexelOrigin.x = WarpMod(dx + static_cast<int>(m_TexelOrigin.x), TextureSz);
    m_TexelOrigin.y = WarpMod(dy + static_cast<int>(m_TexelOrigin.y), TextureSz);
}

ClipmapLevelBase::HollowRing ClipmapLevel::GetHollowRing(const Vector2& toc) const
{
    using namespace SimpleMath;

    HollowRing hollow;
    hollow.TrimId = m_TrimPattern;

    const GridInstance ins
    {
        Vector2(m_GridSpacing),
        Vector2(m_GridSpacing) * m_GridOrigin,
        Vector2(m_TexelOrigin.x, m_TexelOrigin.y) * OneOverSz,
        toc,
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
        block.TextureOffsetCoarser += Trait::LocalOffset[i] * OneOverSz * 0.5f;
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


ClipmapLevelBase::SolidSquare ClipmapLevel::GetSolidSquare(const Vector2& toc) const
{
    using namespace SimpleMath;
    SolidSquare solid;

    const GridInstance ins
    {
        Vector2(m_GridSpacing),
        (Vector2(m_GridSpacing) * m_GridOrigin),
        (Vector2(m_TexelOrigin.x, m_TexelOrigin.y) * OneOverSz),
        toc,
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
    return m_HeightSrc->GetHeight(m_GridOrigin.x + 128, m_GridOrigin.y + 128);
}

Vector2 ClipmapLevel::GetFinerOffset() const
{
    return (m_TexelOrigin + FootprintTrait<InteriorTrim>::FinerOffset[m_TrimPattern]) * OneOverSz;
}
