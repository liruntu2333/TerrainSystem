#define NOMINMAX
#include "TerrainSystem.h"

#include <fstream>
#include <iostream>
#include <set>

#include "../HeightMapSplitter/ThreadPool.h"

constexpr size_t PATCH_NY = 32;
constexpr size_t PATCH_NX = 32;

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

TerrainSystem::TerrainSystem(std::filesystem::path path, ID3D11Device* device) : m_Path(std::move(path))
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

    m_Height = std::make_unique<Texture2D>(device, m_Path.string() + "/height.dds");
    m_Height->CreateViews(device);
    m_Normal = std::make_unique<Texture2D>(device, m_Path.string() + "/normal.dds");
    m_Normal->CreateViews(device);
    m_Albedo = std::make_unique<Texture2D>(device, m_Path.string() + "/albedo.dds");
    m_Albedo->CreateViews(device);

    std::ifstream boundsFile(m_Path.u8string() + "/bounds.json");
    if (!boundsFile.is_open())
        throw std::runtime_error("Failed to open \"bounds.json\".");
    nlohmann::json j;
    boundsFile >> j;
    m_BoundTree = std::make_unique<BoundTree>(j);
}

TerrainSystem::RenderResource TerrainSystem::GetPatchResources(
    const XMINT2& camXyForCull, const BoundingFrustum& frustumLocal,
    std::vector<BoundingBox>& bbs, ID3D11Device* device) const
{
    std::vector<int> visible;
    std::vector<int> lods;
    std::function<void(const BoundTree::Node* node)> recursiveCull = [&](const BoundTree::Node* node)
    {
        if (node == nullptr) return;

        const auto& [minH, maxH, h, w, x, y, id] = node->m_Bound;
        const auto extents = Vector3(
            w * 0.5f,
            (maxH - minH) * PATCH_HEIGHT_RANGE * 0.5f,
            h * 0.5f);
        const auto center = Vector3(
            (x - camXyForCull.x) * PATCH_SIZE + extents.x,
            (minH + maxH) * 0.5f * PATCH_HEIGHT_RANGE,
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

    RenderResource r;
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
    return r;
}
