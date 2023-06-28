#define NOMINMAX
#include "ClipmapLevel.h"

#include <DirectXColors.h>

#include "D3DHelper.h"
#include "Vertex.h"
#include <directxtk/BufferHelpers.h>
#include <DirectXPackedVector.h>

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

    enum TrimPattern : int
    {
        Lt = 0,
        Rt,
        Rb,
        Lb,
    };

    Vector2 operator+(const Vector2& lhs, const XMINT2& rhs)
    {
        return { lhs.x + rhs.x, lhs.y + rhs.y };
    }

    XMINT2 operator*(const XMINT2& lhs, int rhs)
    {
        return { lhs.x * rhs, lhs.y * rhs };
    }

    XMINT2 operator+(const XMINT2& lhs, const Vector2& rhs)
    {
        return { lhs.x + static_cast<int>(rhs.x), lhs.y + static_cast<int>(rhs.y) };
    }

    Vector2 operator*(const Vector2& lhs, const XMINT2& rhs)
    {
        return { lhs.x * rhs.x, lhs.y * rhs.y };
    }

    XMINT2 operator-(const XMINT2& lhs, const XMINT2& rhs)
    {
        return { lhs.x - rhs.x, lhs.y - rhs.y };
    }

    XMINT2 operator+(const XMINT2& lhs, const XMINT2& rhs)
    {
        return { lhs.x + rhs.x, lhs.y + rhs.y };
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

    loadVb(path / "trim_lb.vtx", TrimVb[0]);
    loadIb(path / "trim_lb.idx", TrimIb[0], TrimIdxCnt);
    loadVb(path / "trim_rb.vtx", TrimVb[1]);
    loadIb(path / "trim_rb.idx", TrimIb[1], TrimIdxCnt);
    loadVb(path / "trim_rt.vtx", TrimVb[2]);
    loadIb(path / "trim_rt.idx", TrimIb[2], TrimIdxCnt);
    loadVb(path / "trim_lt.vtx", TrimVb[3]);
    loadIb(path / "trim_lt.idx", TrimIb[3], TrimIdxCnt);

    loaded = true;
}


ClipmapLevel::ClipmapLevel(
    int l,
    const std::shared_ptr<HeightMap>& src, const std::shared_ptr<ClipmapTexture>& hTex)
    : m_Level(l), m_GridSpacing(std::powf(2, l)),
    m_GridOrigin(-128, -126), m_TexelOrigin(0, 0),
    m_Ticker(0.5f, -0.5f), m_HeightSrc(src), m_HeightTex(hTex), m_ArraySlice(l) {}

void ClipmapLevel::UpdateOffset(const Vector2& dView)
{
    m_Ticker += dView / m_GridSpacing * 0.5f;
    const Vector2 ofs(std::round(m_Ticker.x), std::round(m_Ticker.y));

    const auto ofsMul2 = ofs * 2;
    m_GridOrigin = m_GridOrigin + ofsMul2;

    m_Ticker -= ofs;
}

void ClipmapLevel::UpdateTexture(ID3D11DeviceContext* context)
{
    const XMINT2 dGrid = m_GridOrigin - m_MappedOrigin;
    if (dGrid.x == 0 && dGrid.y == 0) return;

    // const MapGuard hm(context, m_HeightTex->GetTexture(),
    //     D3D11CalcSubresource(0, m_ArraySlice, 1), D3D11_MAP_WRITE, 0);

    std::vector<uint16_t> hm(TextureSz * TextureSz);
    const auto dst = reinterpret_cast<uint16_t*>(hm.data());
    constexpr int dw = TextureN;
    constexpr int dh = TextureN;

    const auto src = m_HeightSrc->GetData();
    const int sw = m_HeightSrc->GetWidth();
    const int sh = m_HeightSrc->GetHeight();

    auto copyRectWarp = [src, dst, sw, sh](
        const int dx, const int dy,
        const int dox, const int doy,          // destination origin
        const int sox, const int soy)     // source origin
    {
        assert(dx >= 0 && dy >= 0 && "should update from lt to rb");
        for (int y = 0; y < dy; ++y)
        {
            for (int x = 0; x < dx; ++x)
            {
                dst[PositiveMod(doy + y, TextureSz) * TextureSz +
                        PositiveMod(dox + x, TextureSz)] =
                    src[PositiveMod(soy + y, sh) * sw +
                        PositiveMod(sox + x, sw)];
            }
        }
    };

    //if (std::abs(dGrid.x) >= dw || std::abs(dGrid.y) >= dh) // update full tex anyway
    {
        m_TexelOrigin = { 0, 0 };
        copyRectWarp(dw, dh,
            0, 0,
            m_GridOrigin.x, m_GridOrigin.y);
        m_MappedOrigin = m_GridOrigin;
        context->UpdateSubresource(m_HeightTex->GetTexture(),
            D3D11CalcSubresource(0, m_ArraySlice, 1),
            nullptr, hm.data(), TextureSz * sizeof uint16_t,
            hm.size() * sizeof uint16_t);
        return;
    }

    const int& dx = dGrid.x;
    const int& dy = dGrid.y;
    const int& dox = m_TexelOrigin.x;
    const int& doy = m_TexelOrigin.y;
    const int& sox = m_MappedOrigin.x;
    const int& soy = m_MappedOrigin.y;
    // update dy * dw first for cache coherence
    copyRectWarp(dw, std::abs(dy),
        dox + dx, doy + (dy >= 0 ? dh : dy),
        sox + dx, soy + (dy >= 0 ? dh : dy));
    // then update dx * (dh - abs(dy))
    copyRectWarp(std::abs(dx), dh - std::abs(dy),
        dox + (dx >= 0 ? dw : dx), doy + (dy >= 0 ? dy : 0),
        sox + (dx >= 0 ? dw : dx), soy + (dy >= 0 ? dy : 0));

    m_MappedOrigin = m_GridOrigin;
    m_TexelOrigin = m_TexelOrigin + dGrid;
    m_TexelOrigin.x = PositiveMod(m_TexelOrigin.x, TextureSz);
    m_TexelOrigin.y = PositiveMod(m_TexelOrigin.y, TextureSz);
}

ClipmapLevelBase::HollowRing ClipmapLevel::GetHollowRing() const
{
    using namespace SimpleMath;

    TrimPattern trimType;
    if (m_Ticker.x >= 0 && m_Ticker.y < 0) trimType = Lt;
    else if (m_Ticker.x < 0 && m_Ticker.y < 0) trimType = Rt;
    else if (m_Ticker.x < 0 && m_Ticker.y >= 0) trimType = Rb;
    else /*if (m_Ticker.x >= 0 && m_Ticker.y >= 0)*/ trimType = Lb;

    HollowRing hollow;
    hollow.TrimId = trimType;
    const Vector2 xzOfs = Vector2(m_GridSpacing) * m_GridOrigin;
    const Vector2 uvOfs = Vector2(m_TexelOrigin.x, m_TexelOrigin.y) * OneOverSz;

    for (auto&& block : hollow.Blocks)
    {
        block.GridScale = Vector2(m_GridSpacing);
        block.TexelScale = Vector2(OneOverSz);
        block.WorldOffset = xzOfs;
        block.TextureOffset = uvOfs;
        block.Level = m_Level;
    }
    hollow.RingFixUp.GridScale = hollow.Trim.GridScale = Vector2(m_GridSpacing);
    hollow.RingFixUp.TexelScale = hollow.Trim.TexelScale = Vector2(OneOverSz);
    hollow.RingFixUp.WorldOffset = hollow.Trim.WorldOffset = xzOfs;
    hollow.RingFixUp.TextureOffset = hollow.Trim.TextureOffset = uvOfs;
    hollow.RingFixUp.Level = hollow.Trim.Level = m_Level;

    for (int i = 0; i < 12; ++i)
    {
        GridInstance& block = hollow.Blocks[i];
        using Trait = FootprintTrait<Block>;
        block.WorldOffset += Trait::LocalOffset[i] * m_GridSpacing;
        block.TextureOffset += Trait::LocalOffset[i] * OneOverSz;
        block.LocalOffset = XMUINT2(Trait::LocalOffset[i].x, Trait::LocalOffset[i].y);
        block.Color = i == 0 || i == 2 || i == 5 || i == 11 || i == 6 || i == 9
                          ? Trait::Color0
                          : Trait::Color1;
    }

    {
        GridInstance& fixUp = hollow.RingFixUp;
        using Trait = FootprintTrait<RingFixUp>;
        fixUp.LocalOffset = XMUINT2();
        fixUp.Color = Trait::Color;
    }

    {
        GridInstance& trim = hollow.Trim;
        using Trait = FootprintTrait<InteriorTrim>;
        trim.LocalOffset = XMUINT2();
        trim.Color = Trait::Color;
    }

    return std::move(hollow);
}


ClipmapLevelBase::SolidSquare ClipmapLevel::GetSolidSquare() const
{
    using namespace SimpleMath;
    SolidSquare solid;

    const Vector2 xzOfs = Vector2(m_GridSpacing) * m_GridOrigin;
    const Vector2 uvOfs = Vector2(m_TexelOrigin.x, m_TexelOrigin.y) * OneOverSz;

    for (auto&& block : solid.Blocks)
    {
        block.GridScale = Vector2(m_GridSpacing);
        block.TexelScale = Vector2(OneOverSz);
        block.WorldOffset = xzOfs;
        block.TextureOffset = uvOfs;
        block.Level = m_Level;
    }
    solid.RingFixUp.GridScale = solid.Trim[0].GridScale = solid.Trim[1].GridScale = Vector2(m_GridSpacing);
    solid.RingFixUp.TexelScale = solid.Trim[0].TexelScale = solid.Trim[1].TexelScale = Vector2(OneOverSz);
    solid.RingFixUp.WorldOffset = solid.Trim[0].WorldOffset = solid.Trim[1].WorldOffset = xzOfs;
    solid.RingFixUp.TextureOffset = solid.Trim[0].TextureOffset = solid.Trim[1].TextureOffset = uvOfs;
    solid.RingFixUp.Level = solid.Trim[0].Level = solid.Trim[1].Level = m_Level;

    for (int i = 0; i < 16; ++i)
    {
        GridInstance& block = solid.Blocks[i];
        using Trait = FootprintTrait<Block>;
        block.WorldOffset += Trait::LocalOffset[i] * m_GridSpacing;
        block.TextureOffset += Trait::LocalOffset[i] * OneOverSz;
        block.LocalOffset = XMUINT2(Trait::LocalOffset[i].x, Trait::LocalOffset[i].y);
        block.Color = i == 0 || i == 2 || i == 5 || i == 11 || i == 6 || i == 9 || i == 12 || i == 15
                          ? Trait::Color0
                          : Trait::Color1;
    }

    {
        GridInstance& fixUp = solid.RingFixUp;
        using Trait = FootprintTrait<RingFixUp>;
        fixUp.LocalOffset = XMUINT2();
        fixUp.Color = Trait::Color;
    }

    solid.TrimId[0] = 0;
    solid.TrimId[1] = (solid.TrimId[0] + 2) % 4;
    for (auto& trim : solid.Trim)
    {
        using Trait = FootprintTrait<InteriorTrim>;
        trim.LocalOffset = XMUINT2();
        trim.Color = Trait::Color;
    }

    return std::move(solid);
}

float ClipmapLevel::GetHeight() const
{
    return m_HeightSrc->GetHeight(m_GridOrigin.x, m_GridOrigin.y);
}
