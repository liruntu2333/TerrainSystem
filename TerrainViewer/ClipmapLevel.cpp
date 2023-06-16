#include "ClipmapLevel.h"

#include <DirectXColors.h>

#include "D3DHelper.h"
#include "MeshVertex.h"
#include <directxtk/BufferHelpers.h>

using namespace DirectX;
using namespace SimpleMath;

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
    };

    template <>
    struct FootprintTrait<RingFixUp> : FootprintTraitBase
    {
        static constexpr Vector2 LocalOffset = { 0, 0 };
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

ClipmapLevelBase::HollowRing ClipmapLevel::GetHollowRing(
    Vector2& wldOfs, Vector2& texOfs) const
{
    using namespace SimpleMath;
    HollowRing ring;
    for (int i = 0; i < 12; ++i)
    {
        GridInstance& block = ring.Blocks[i];
        using Trait = FootprintTrait<Block>;
        block.GridScale = Vector2(m_Scale);
        block.GridOffset = Trait::LocalOffset[i] * m_Scale + wldOfs;
        block.TexelScale = Vector2(m_Scale / 8192.0f);
        block.TextureOffset = Trait::LocalOffset[i] * m_Scale / 8192.0f + texOfs;
        block.Color = i == 0 || i == 2 || i == 5 || i == 11 || i == 6 || i == 9
                          ? Colors::DarkGreen
                          : Colors::Purple;
    }

    {
        GridInstance& fixUp = ring.RingFixUp;
        using Trait = FootprintTrait<RingFixUp>;
        fixUp.GridScale = Vector2(m_Scale);
        fixUp.GridOffset = Trait::LocalOffset * m_Scale + wldOfs;
        fixUp.TexelScale = Vector2(m_Scale / 8192.0f);
        fixUp.TextureOffset = Trait::LocalOffset / 8192.0f + texOfs;
        fixUp.Color = Colors::Gold;
    }

    ring.TrimId = m_Level % 4;
    {
        GridInstance& trim = ring.Trim;
        using Trait = FootprintTrait<InteriorTrim>;
        trim.GridScale = Vector2(m_Scale);
        trim.GridOffset = Trait::LocalOffset * m_Scale + wldOfs;
        trim.TexelScale = Vector2(m_Scale / 8192.0f);
        trim.TextureOffset = Trait::LocalOffset * m_Scale / 8192.0f + texOfs;
        trim.Color = Colors::RoyalBlue;

        wldOfs += Trait::FinerOffset[ring.TrimId] * m_Scale;
        texOfs += Trait::FinerOffset[ring.TrimId] * m_Scale / 8192.0f;
    }
    return std::move(ring);
}

ClipmapLevelBase::FullSquare ClipmapLevel::GetSolidSquare(
    Vector2& wldOfs, Vector2& texOfs) const
{
    FullSquare sqr;
    using namespace SimpleMath;
    constexpr float textureSize = 8192.0f;
    for (int i = 0; i < 16; ++i)
    {
        GridInstance& block = sqr.Blocks[i];
        using Trait = FootprintTrait<Block>;
        block.GridScale = Vector2(m_Scale);
        block.GridOffset = Trait::LocalOffset[i] * m_Scale + wldOfs;
        block.TexelScale = Vector2(m_Scale / textureSize);
        block.TextureOffset = Trait::LocalOffset[i] * m_Scale / textureSize + texOfs;
        block.Color = i == 0 || i == 2 || i == 5 || i == 11 || i == 6 || i == 9 || i == 12 || i == 15
                          ? Colors::DarkGreen
                          : Colors::Purple;
    }

    {
        GridInstance& fixUp = sqr.RingFixUp;
        using Trait = FootprintTrait<RingFixUp>;
        fixUp.GridScale = Vector2(m_Scale);
        fixUp.GridOffset = Trait::LocalOffset * m_Scale + wldOfs;
        fixUp.TexelScale = Vector2(m_Scale / textureSize);
        fixUp.TextureOffset = Trait::LocalOffset * m_Scale / textureSize + texOfs;
        fixUp.Color = Colors::Gold;
    }

    sqr.TrimId[0] = m_Level % 4;
    sqr.TrimId[1] = (sqr.TrimId[0] + 2) % 4;
    for (auto& trim : sqr.Trim)
    {
        using Trait = FootprintTrait<InteriorTrim>;
        trim.GridScale = Vector2(m_Scale);
        trim.GridOffset = Trait::LocalOffset * m_Scale + wldOfs;
        trim.TexelScale = Vector2(m_Scale / textureSize);
        trim.TextureOffset = Trait::LocalOffset * m_Scale / textureSize + texOfs;
        trim.Color = Colors::RoyalBlue;
    }

    wldOfs += FootprintTrait<Block>::LocalOffset[15] * m_Scale;
    texOfs += FootprintTrait<Block>::LocalOffset[15] * m_Scale / textureSize;

    return std::move(sqr);
}
