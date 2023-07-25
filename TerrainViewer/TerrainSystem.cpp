#define NOMINMAX
#include "TerrainSystem.h"

#include <fstream>
#include <iostream>
#include <set>

#include "BitMap.h"
#include "../HeightMapSplitter/ThreadPool.h"

constexpr size_t PATCH_NY = 8;
constexpr size_t PATCH_NX = 8;

using namespace DirectX;
using namespace SimpleMath;

namespace
{
    const PackedVector::XMCOLOR G_COLORS[] =
    {
        { 1.000000000f, 0.000000000f, 0.000000000f, 1.000000000f },
        { 0.000000000f, 0.501960814f, 0.000000000f, 1.000000000f },
        { 0.000000000f, 0.000000000f, 1.000000000f, 1.000000000f },
        { 1.000000000f, 1.000000000f, 0.000000000f, 1.000000000f },
        { 0.000000000f, 1.000000000f, 1.000000000f, 1.000000000f },
    };

    const std::set G_DISTANCES =
    {
        1020.0f,
        2040.0f,
        3060.0f,
    };
}

void TerrainSystem::InitMeshPatches(ID3D11Device* device)
{
    std::vector<std::future<std::shared_ptr<Patch>>> results;
    for (int y = 0; y < PATCH_NY; ++y)
        for (int x = 0; x < PATCH_NX; ++x)
            results.emplace_back(g_ThreadPool.enqueue([this, device, x, y]
            {
                return std::make_shared<Patch>(m_Path, x, y, device);
            }));

    for (auto& result : results)
    {
        auto p = result.get();
        int id = p->m_X + p->m_Y * PATCH_NX;
        m_Patches.emplace(id, std::move(p));
    }

    std::ifstream boundsFile(m_Path / "bounds.json");
    if (!boundsFile.is_open())
        throw std::runtime_error("Failed to open bounds.json.");
    nlohmann::json j;
    boundsFile >> j;
    m_BoundTree = std::make_unique<BoundTree>(j);
}

void TerrainSystem::InitClipTextures(ID3D11Device* device)
{
    m_HeightCm = std::make_shared<ClipmapTexture>(device,
        CD3D11_TEXTURE2D_DESC(ClipmapLevel::HeightFormat,
            ClipmapLevel::TextureSz * ClipmapLevel::TextureScaleHeight,
            ClipmapLevel::TextureSz * ClipmapLevel::TextureScaleHeight,
            LevelCount, 1));
    m_HeightCm->CreateViews(device);

    {
        CD3D11_TEXTURE2D_DESC desc(ClipmapLevel::AlbedoFormat,
            ClipmapLevel::TextureSz * ClipmapLevel::TextureScaleAlbedo,
            ClipmapLevel::TextureSz * ClipmapLevel::TextureScaleAlbedo, LevelCount, 1);
#ifdef HARDWARE_FILTERING
    desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
    desc.MipLevels = std::log2(ClipmapLevel::TextureSz * ClipmapLevel::TextureScaleAlbedo);
    desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
#endif
        m_AlbedoCm = std::make_shared<ClipmapTexture>(device, desc);
        m_AlbedoCm->CreateViews(device);
    }

    {
        CD3D11_TEXTURE2D_DESC desc(ClipmapLevel::NormalFormat,
            ClipmapLevel::TextureSz * ClipmapLevel::TextureScaleNormal,
            ClipmapLevel::TextureSz * ClipmapLevel::TextureScaleNormal, LevelCount, 1);
#ifdef HARDWARE_FILTERING
    desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
    desc.MipLevels = std::log2(ClipmapLevel::TextureSz * ClipmapLevel::TextureScaleNormal);
    desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
#endif
        m_NormalCm = std::make_shared<ClipmapTexture>(device, desc);
        m_NormalCm->CreateViews(device);
    }

    ClipmapLevel::BindTexture(m_HeightCm, m_AlbedoCm, m_NormalCm);
}

void TerrainSystem::InitClipmapLevels(ID3D11Device* device, const Vector3& view)
{
    ClipmapLevelBase::LoadFootprintGeometry(m_Path / "clipmap", device);
    const auto hm = std::make_shared<TiledMap<HeightMap>>(std::vector {
        std::make_shared<HeightMap>(m_Path / "height0.dds"),
        std::make_shared<HeightMap>(m_Path / "height1.dds"),
        std::make_shared<HeightMap>(m_Path / "height2.dds"),
        std::make_shared<HeightMap>(m_Path / "height3.dds"),
    });

    const auto sm = std::make_shared<TiledMap<SplatMap>>(std::vector {
        std::make_shared<SplatMap>(m_Path / "splat0.dds"),
        std::make_shared<SplatMap>(m_Path / "splat1.dds"),
        std::make_shared<SplatMap>(m_Path / "splat2.dds"),
        std::make_shared<SplatMap>(m_Path / "splat3.dds"),
    });

    const auto nm = std::make_shared<TiledMap<NormalMap>>(std::vector {
        std::make_shared<NormalMap>(m_Path / "normal0.dds"),
        std::make_shared<NormalMap>(m_Path / "normal1.dds"),
        std::make_shared<NormalMap>(m_Path / "normal2.dds"),
        std::make_shared<NormalMap>(m_Path / "normal3.dds"),
    });

    const std::vector albedo =
    {
        std::make_shared<AlbedoMap>(m_Path / "texture_can/snow_a.dds"),
        std::make_shared<AlbedoMap>(m_Path / "texture_can/rock_a.dds"),
        std::make_shared<AlbedoMap>(m_Path / "texture_can/grass_a.dds"),
        std::make_shared<AlbedoMap>(m_Path / "texture_can/ground_a.dds"),
    };

    const std::vector normal =
    {
        std::make_shared<NormalMap>(m_Path / "texture_can/snow_n.dds"),
        std::make_shared<NormalMap>(m_Path / "texture_can/rock_n.dds"),
        std::make_shared<NormalMap>(m_Path / "texture_can/grass_n.dds"),
        std::make_shared<NormalMap>(m_Path / "texture_can/ground_n.dds"),
    };

    InitClipTextures(device);
    m_SrcManager->BindSource(hm, sm, nm, albedo, normal);
    ClipmapLevel::BindSourceManager(m_SrcManager);

    for (int i = 0; i < LevelCount; ++i)
    {
        m_Levels.emplace_back(i, LevelZeroScale * std::powf(2.0f, i), view);
    }
}

TerrainSystem::TerrainSystem(
    const Vector3& view,
    std::filesystem::path path,
    ID3D11Device* device) :
    m_SrcManager(std::make_unique<BitmapManager>()), m_Path(std::move(path))
{
    // InitMeshPatches(device);

    // m_Height = std::make_unique<Texture2D>(device, m_Path / "height.dds");
    // m_Height->CreateViews(device);
    // m_Normal = std::make_unique<Texture2D>(device, m_Path / "normal.dds");
    // m_Normal->CreateViews(device);
    // m_Albedo = std::make_unique<Texture2D>(device, m_Path / "albedo.dds");
    // m_Albedo->CreateViews(device);

    InitClipmapLevels(device, view);
}

void TerrainSystem::ResetClipmapTexture()
{
    for (auto&& level : m_Levels)
    {
        level.m_GridOrigin = Vector2(static_cast<float>(std::numeric_limits<int>::min() >> 1));
    }
}

TerrainSystem::PatchRenderResource TerrainSystem::GetPatchResources(
    const XMINT2& camXyForCull, const BoundingFrustum& frustumLocal,
    const float yScale, std::vector<BoundingBox>& bbs, ID3D11Device* device) const
{
    std::vector<int> visible;
    std::vector<int> lods;
    std::function<void(const BoundTree::Node* node)> recursiveCull = [&](const BoundTree::Node* node)
    {
        if (node == nullptr) return;

        const auto& [minH, maxH, h, w, x, y, id] = node->m_Bound;
        const auto extents = Vector3(
            w * 0.5f,
            (maxH - minH) * yScale * 0.5f,
            h * 0.5f);
        const auto center = Vector3(
            (x - camXyForCull.x) * PATCH_SIZE + extents.x,
            (minH + maxH) * 0.5f * yScale + 1000.0f,
            (y - camXyForCull.y) * PATCH_SIZE + extents.z);
        const BoundingBox bb(center, extents);

        if (frustumLocal.Contains(bb) == DISJOINT) return;

        if (id != -1)
        {
            bbs.emplace_back(bb);
            visible.emplace_back(id);

            const auto dist = (frustumLocal.Origin - center).Length();
            const auto upper = G_DISTANCES.upper_bound(dist);
            const int lod = upper == G_DISTANCES.end()
                                ? G_DISTANCES.size() - 1
                                : std::distance(G_DISTANCES.begin(), upper);
            lods.emplace_back(lod);

            return;
        }

        for (const auto& child : node->m_Children)
            recursiveCull(child.get());
    };

    recursiveCull(m_BoundTree->m_Root.get());

    PatchRenderResource r;
    // r.Height = m_Height->GetSrv();
    // r.Normal = m_Normal->GetSrv();
    // r.Albedo = m_Albedo->GetSrv();
    for (int i = 0; i < visible.size(); ++i)
    {
        const auto id = visible[i];
        const auto lod = lods[i];
        if (m_Patches.find(id) != m_Patches.end())
        {
            auto rr = m_Patches.at(id)->GetResource(m_Path, lod, device);
            rr.Color = G_COLORS[lod];
            r.Patches.emplace_back(rr);
        }
    }
    return std::move(r);
}

TerrainSystem::ClipmapRenderResource TerrainSystem::TickClipmap(
    const BoundingFrustum& frustum,
    const float yScale, ID3D11DeviceContext* context, const int blendMode, std::vector<BoundingBox>& bounding)
{
    int budget = ClipmapLevel::TextureN * ClipmapLevel::TextureN;
    for (int i = LevelCount - 1; i >= 0; --i)
    {
        m_Levels[i].TickTransform(frustum.Origin, budget, yScale, blendMode);
    }

    auto rr = GetClipmapRenderResource(frustum, yScale, bounding);
    ClipmapLevel::UpdateTexture(context);
    return std::move(rr);
}

TerrainSystem::ClipmapRenderResource TerrainSystem::GetClipmapRenderResource(
    const BoundingFrustum& frustum, float hScl, std::vector<BoundingBox>& bounding) const
{
    ClipmapRenderResource r;
    // r.Height = m_Height->GetSrv();
    r.HeightCm = m_HeightCm->GetSrv();
    r.NormalCm = m_NormalCm->GetSrv();
    r.AlbedoCm = m_AlbedoCm->GetSrv();

    r.BlockVb = ClipmapLevelBase::BlockVb.Get();
    r.BlockIb = ClipmapLevelBase::BlockIb.Get();
    r.BlockIdxCnt = ClipmapLevelBase::BlockIdxCnt;

    r.RingVb = ClipmapLevelBase::RingFixUpVb.Get();
    r.RingIb = ClipmapLevelBase::RingFixUpIb.Get();
    r.RingIdxCnt = ClipmapLevelBase::RingIdxCnt;

    for (int i = 0; i < 4; ++i)
    {
        r.TrimVb[i] = ClipmapLevelBase::TrimVb[i].Get();
        r.TrimIb[i] = ClipmapLevelBase::TrimIb[i].Get();
    }
    r.TrimIdxCnt = ClipmapLevelBase::TrimIdxCnt;

    std::vector<GridInstance> blocks;
    std::vector<GridInstance> rings;
    std::vector<GridInstance> trims[4];

    // from coarse to fine
    int lvl = LevelCount - 1;
    for (; lvl > 0 && (m_Levels[lvl].IsActive() && m_Levels[lvl - 1].IsActive()); --lvl)
    {
        const auto ofsCoarse = lvl < LevelCount - 1
                                   ? m_Levels[lvl + 1].GetFinerUvOffset(m_Levels[lvl].GetWorldOffset())
                                   : m_Levels[lvl].GetUvOffset(); // TODO
        auto [block, ring, trim, tid] =
            m_Levels[lvl].GetHollowRing(ofsCoarse, m_Levels[lvl - 1].GetWorldOffset(), frustum, hScl, bounding);
        for (const auto& b : block) blocks.emplace_back(b);
        rings.emplace_back(ring);
        trims[tid].emplace_back(trim);
    }

    // finest
    {
        const auto ofsCoarse = lvl < LevelCount - 1
                                   ? m_Levels[lvl + 1].GetFinerUvOffset(m_Levels[lvl].GetWorldOffset())
                                   : m_Levels[lvl].GetUvOffset(); // TODO
        auto [block, ring, trim] = m_Levels[lvl].GetSolidSquare(ofsCoarse, frustum, hScl, bounding);
        for (const auto& b : block) blocks.emplace_back(b);
        rings.emplace_back(ring);
        for (int i = 0; i < 2; ++i) trims[i * 2].emplace_back(trim[i]);
    }

    auto& all = r.Grids;
    all.reserve(blocks.size() + rings.size() + trims[0].size() + trims[1].size() + trims[2].size() + trims[3].size());
    std::copy(blocks.begin(), blocks.end(), std::back_inserter(all));
    std::copy(rings.begin(), rings.end(), std::back_inserter(all));
    for (int i = 0; i < 4; ++i)
    {
        std::copy(trims[i].begin(), trims[i].end(), std::back_inserter(all));
    }

    r.BlockInstanceStart = 0;
    r.RingInstanceStart = r.BlockInstanceStart + blocks.size();
    r.TrimInstanceStart[0] = r.RingInstanceStart + rings.size();
    r.TrimInstanceStart[1] = r.TrimInstanceStart[0] + trims[0].size();
    r.TrimInstanceStart[2] = r.TrimInstanceStart[1] + trims[1].size();
    r.TrimInstanceStart[3] = r.TrimInstanceStart[2] + trims[2].size();

    return std::move(r);
}
