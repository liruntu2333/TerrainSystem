#include "ClipmapLevel.h"

#include <DirectXColors.h>

#include "D3DHelper.h"
#include "Vertex.h"
#include <directxtk/BufferHelpers.h>
#include <DirectXPackedVector.h>

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
        static constexpr int M = ClipMapM;
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
        static constexpr Vector2 LocalOffset = { 0, 0 };
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
        static constexpr Vector2 LocalOffset = { 0, 0 };

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
                return GridVertex(Vector2(v.PositionX, v.PositionY));
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


ClipmapLevel::ClipmapLevel(int l)
    : m_Level(l), m_Scale(1u << l),
    m_WorldOffset(Vector2(-128, -126)), m_TexelOffset(Vector2(0.0))
{
    m_Ticker = Vector2(0.0f, 0.0f);
}

void ClipmapLevel::Update(const Vector2& dView)
{
    m_Ticker += dView / m_Scale;

    Vector2 fdp;
    fdp.x = std::floor(m_Ticker.x / 2);
    fdp.y = std::floor(m_Ticker.y / 2);

    m_WorldOffset = m_WorldOffset + fdp * 2;
    m_TexelOffset = m_TexelOffset + fdp * 2;

    m_Ticker.x -= fdp.x * 2;
    m_Ticker.y -= fdp.y * 2;

    // const bool finerXAsync = abs(m_Ticker.x) >= 0.5f;
    // const bool finerYAsync = abs(m_Ticker.y) >= 0.5f;
    //
    // const bool finerToLft = m_Ticker.x < 0 && finerXAsync;
    // const bool finerToRht = m_Ticker.x > 0 && finerXAsync;
    // const bool finerToTop = m_Ticker.y < 0 && finerYAsync;
    // const bool finerToBtm = m_Ticker.y > 0 && finerYAsync;
    // const int finerCorner =
    //     (finerToLft && finerToTop)
    //         ? 0
    //         : (finerToRht && finerToTop)
    //               ? 1
    //               : (finerToRht && finerToBtm)
    //                     ? 2
    //                     : (finerToLft && finerToBtm)
    //                           ? 3
    //                           : -1;
    //
    // m_TrimId = (finerCorner + 2) % 4;
}

ClipmapLevelBase::HollowRing ClipmapLevel::GetHollowRing(
    float txlScl) const
{
    using namespace SimpleMath;

    HollowRing hollow;
    hollow.TrimId = m_TrimId;
    const auto finerOfs = m_WorldOffset * m_Scale;
    const auto texOfs = m_TexelOffset * txlScl;

    for (auto&& block : hollow.Blocks)
    {
        block.ScaleFactor = Vector2(m_Scale, txlScl);
        block.Level = m_Level;
    }
    hollow.RingFixUp.ScaleFactor = hollow.Trim.ScaleFactor = Vector2(m_Scale, txlScl);
    hollow.RingFixUp.Level = hollow.Trim.Level = m_Level;

    for (int i = 0; i < 12; ++i)
    {
        GridInstance& block = hollow.Blocks[i];
        using Trait = FootprintTrait<Block>;
        block.OffsetInWorld = Trait::LocalOffset[i] * m_Scale + finerOfs;
        block.TextureOffset = Trait::LocalOffset[i] * txlScl + texOfs;
        block.OffsetInLevel = Trait::LocalOffset[i];
        block.Color = i == 0 || i == 2 || i == 5 || i == 11 || i == 6 || i == 9
                          ? Trait::Color0
                          : Trait::Color1;
    }

    {
        GridInstance& fixUp = hollow.RingFixUp;
        using Trait = FootprintTrait<RingFixUp>;
        fixUp.OffsetInWorld = Trait::LocalOffset * m_Scale + finerOfs;
        fixUp.TextureOffset = Trait::LocalOffset * txlScl + texOfs;
        fixUp.OffsetInLevel = Trait::LocalOffset;
        fixUp.Color = Trait::Color;
    }

    {
        GridInstance& trim = hollow.Trim;
        using Trait = FootprintTrait<InteriorTrim>;
        trim.OffsetInWorld = Trait::LocalOffset * m_Scale + finerOfs;
        trim.TextureOffset = Trait::LocalOffset * txlScl + texOfs;
        trim.OffsetInLevel = Trait::LocalOffset;
        trim.Color = Trait::Color;
    }

    return std::move(hollow);
}


ClipmapLevelBase::SolidSquare ClipmapLevel::GetSolidSquare(
    float txlScl) const
{
    using namespace SimpleMath;
    SolidSquare solid;

    const auto finerOfs = m_WorldOffset * m_Scale;
    const auto texOfs = m_TexelOffset * txlScl;

    for (auto&& block : solid.Blocks)
    {
        block.ScaleFactor = Vector2(m_Scale, txlScl);
        block.Level = m_Level;
    }
    solid.RingFixUp.ScaleFactor = solid.Trim[0].ScaleFactor = solid.Trim[1].ScaleFactor = Vector2(m_Scale, txlScl);
    solid.RingFixUp.Level = solid.Trim[0].Level = solid.Trim[1].Level = m_Level;

    for (int i = 0; i < 16; ++i)
    {
        GridInstance& block = solid.Blocks[i];
        using Trait = FootprintTrait<Block>;
        block.OffsetInWorld = Trait::LocalOffset[i] * m_Scale + finerOfs;
        block.TextureOffset = Trait::LocalOffset[i] * txlScl + texOfs;
        block.OffsetInLevel = Trait::LocalOffset[i];
        block.Color = i == 0 || i == 2 || i == 5 || i == 11 || i == 6 || i == 9 || i == 12 || i == 15
                          ? Trait::Color0
                          : Trait::Color1;
    }

    {
        GridInstance& fixUp = solid.RingFixUp;
        using Trait = FootprintTrait<RingFixUp>;
        fixUp.OffsetInWorld = Trait::LocalOffset * m_Scale + finerOfs;
        fixUp.TextureOffset = Trait::LocalOffset * txlScl + texOfs;
        fixUp.OffsetInLevel = Trait::LocalOffset;
        fixUp.Color = Trait::Color;
    }

    solid.TrimId[0] = m_TrimId;
    solid.TrimId[1] = (solid.TrimId[0] + 2) % 4;
    for (auto& trim : solid.Trim)
    {
        using Trait = FootprintTrait<InteriorTrim>;
        trim.OffsetInWorld = Trait::LocalOffset * m_Scale + finerOfs;
        trim.TextureOffset = Trait::LocalOffset * txlScl + texOfs;
        trim.OffsetInLevel = Trait::LocalOffset;
        trim.Color = Trait::Color;
    }

    return std::move(solid);
}
