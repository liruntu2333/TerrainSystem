#define NOMINMAX
#include "TerrainSystem.h"

#include <fstream>
#include <iostream>
#include <set>

#include "HeightMap.h"
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

void TerrainSystem::InitClipmapLevels(ID3D11Device* device)
{
    ClipmapLevelBase::LoadFootprintGeometry(m_Path / "clipmap", device);
    auto hm = std::make_shared<HeightMap>(m_Path / "height.png");

    m_HeightCm = std::make_shared<ClipmapTexture>(device,
        CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R16_UNORM,
            ClipmapLevel::TextureSz, ClipmapLevel::TextureSz, LevelCount, 1,
            D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT));
    m_HeightCm->CreateViews(device);

    for (int i = LevelMin; i < LevelMin + LevelCount; ++i)
    {
        m_Levels.emplace_back(i, hm, m_HeightCm);
        hm = hm->GetCoarser();
    }
}

TerrainSystem::TerrainSystem(std::filesystem::path path, ID3D11Device* device) : m_Path(std::move(path))
{
    // InitMeshPatches(device);

    m_Height = std::make_unique<Texture2D>(device, m_Path / "height.dds");
    m_Height->CreateViews(device);
    m_Normal = std::make_unique<Texture2D>(device, m_Path / "normal.dds");
    m_Normal->CreateViews(device);
    m_Albedo = std::make_unique<Texture2D>(device, m_Path / "albedo.dds");
    m_Albedo->CreateViews(device);

    InitClipmapLevels(device);
}

TerrainSystem::PatchRenderResource TerrainSystem::GetPatchResources(
    const XMINT2& camXyForCull, const BoundingFrustum& frustumLocal,
    float yScale, std::vector<BoundingBox>& bbs, ID3D11Device* device) const
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
    r.Height = m_Height->GetSrv();
    r.Normal = m_Normal->GetSrv();
    r.Albedo = m_Albedo->GetSrv();
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

TerrainSystem::ClipmapRenderResource TerrainSystem::GetClipmapResources(
    const Vector3& view3, const Vector3& dView, float yScale, ID3D11DeviceContext* context)
{
    ClipmapRenderResource r;
    // r.Height = m_Height->GetSrv();
    r.HeightCm = m_HeightCm->GetSrv();
    r.Normal = m_Normal->GetSrv();
    r.Albedo = m_Albedo->GetSrv();

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

    Vector2 view2(view3.x, view3.z);
    const auto dView2 = view2 - m_View;
    m_View = view2;

    for (auto&& level : m_Levels)
    {
        level.UpdateOffset(dView2);
    }

    int lowestActive = LevelMin;
    if (float vh = view3.y - m_Levels[0].GetHeight() * yScale; vh > 0)
    {
        lowestActive = std::clamp(std::log2(vh / (2.5f * 254.0f)),
            static_cast<float>(LevelMin),
            static_cast<float>(LevelMin + LevelCount - 1));
    }

    for (int i = lowestActive; i < LevelCount + LevelMin; ++i)
    {
        m_Levels[i].UpdateTexture(context);
    }

    {
        auto [block, ring, trim, tid] = m_Levels[lowestActive].GetSolidSquare();
        for (const auto& b : block) blocks.emplace_back(b);
        rings.emplace_back(ring);
        for (int i = 0; i < 2; ++i) trims[tid[i]].emplace_back(trim[i]);
    }

    for (int lv = lowestActive + 1; lv < LevelMin + LevelCount; ++lv)
    {
        auto [block, ring, trim, tid] = m_Levels[lv - LevelMin].GetHollowRing();
        for (const auto& b : block) blocks.emplace_back(b);
        rings.emplace_back(ring);
        trims[tid].emplace_back(trim);
    }

    auto& all = r.Grids;
    all.reserve(blocks.size() + rings.size() + trims[0].size() + trims[1].size() + trims[2].size() + trims[3].size());
    all.insert(all.end(), blocks.begin(), blocks.end());
    all.insert(all.end(), rings.begin(), rings.end());
    for (int i = 0; i < 4; ++i) all.insert(all.end(), trims[i].begin(), trims[i].end());

    r.BlockInstanceStart = 0;
    r.RingInstanceStart = r.BlockInstanceStart + blocks.size();
    r.TrimInstanceStart[0] = r.RingInstanceStart + rings.size();
    r.TrimInstanceStart[1] = r.TrimInstanceStart[0] + trims[0].size();
    r.TrimInstanceStart[2] = r.TrimInstanceStart[1] + trims[1].size();
    r.TrimInstanceStart[3] = r.TrimInstanceStart[2] + trims[2].size();

    return std::move(r);
}
