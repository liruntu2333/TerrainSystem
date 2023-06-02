#define NOMINMAX
#include "TerrainSystem.h"

#include <fstream>
#include <iostream>
#include <string>

#include "../HeightMapSplitter/ThreadPool.h"

constexpr size_t PATCH_NY = 32;
constexpr size_t PATCH_NX = 32;

using namespace DirectX;
using namespace SimpleMath;

TerrainSystem::TerrainSystem(const std::filesystem::path& path, ID3D11Device* device)
{
    std::vector<std::future<std::shared_ptr<Patch>>> results;
    for (int x = 0; x < PATCH_NX; ++x)
        for (int y = 0; y < PATCH_NY; ++y)
            results.emplace_back(g_ThreadPool.enqueue([&path, device, this, x, y]
            {
                std::filesystem::path patchPath = path.u8string() + '/' +
                    std::to_string(x) + "_" + std::to_string(y);
                auto patch = std::make_shared<Patch>(patchPath, device);
                std::cout << "Loaded patch " << x << ", " << y << std::endl;
                return patch;
            }));

    for (int i = 0; i < results.size(); ++i)
        m_Patches.emplace(i, results[i].get());

    m_Height = std::make_unique<Texture2D>(device, path.string() + "/height.dds");
    m_Height->CreateViews(device);
    m_Normal = std::make_unique<Texture2D>(device, path.string() + "/normal.dds");
    m_Normal->CreateViews(device);
    m_Albedo = std::make_unique<Texture2D>(device, path.string() + "/albedo.dds");
    m_Albedo->CreateViews(device);

    std::ifstream boundsFile(path.u8string() + "/bounds.json");
    if (!boundsFile.is_open())
        throw std::runtime_error("Failed to open \"bounds.json\".");
    nlohmann::json j;
    boundsFile >> j;
    m_BoundTree = std::make_unique<BoundTree>(j);
}

TerrainSystem::RenderResource TerrainSystem::GetPatchResources(
    const Vector3& worldPos,
    Vector3& localPos,
    const BoundingFrustum& frustum) const
{
    const XMINT2 cameraPatchXy =
    {
        static_cast<int>(worldPos.x / PATCH_SIZE),
        static_cast<int>(worldPos.z / PATCH_SIZE)
    };

    localPos = worldPos - Vector3(cameraPatchXy.x * PATCH_SIZE, 0.0f, cameraPatchXy.y * PATCH_SIZE);

    std::vector<int> visible;
    std::function<void(BoundTree::Node* node)> recursiveCull = [&](BoundTree::Node* node)
    {
        if (node == nullptr) return;

        const auto& [minH, maxH, h, w, x, y, id] = node->m_Bound;
        const auto extents = Vector3(
            w * PATCH_SIZE * 0.5f,
            (maxH - minH) * 0.5f * PATCH_HEIGHT_RANGE,
            h * PATCH_SIZE * 0.5f);
        const auto center = Vector3(
            x * PATCH_SIZE + extents.x,
            (minH + maxH) * 0.5f * PATCH_HEIGHT_RANGE,
            y * PATCH_SIZE - extents.z);

        const BoundingBox bb(center, extents);
        if (frustum.Contains(bb) == DISJOINT) return;

        if (id != -1)
        {
            visible.emplace_back(id);
            return;
        }

        for (const auto& child : node->m_Children)
            recursiveCull(child.get());
    };

    //recursiveCull(m_BoundTree->m_Root.get());

    RenderResource r;
    r.Height = m_Height->GetSrv();
    r.Normal = m_Normal->GetSrv();
    r.Albedo = m_Albedo->GetSrv();
    //for (int id : visible)
    //{
    //    r.Patches.emplace_back(m_Patches.at(id)->GetResource(localPos, cameraPatchXy));
    //}
    for (const auto & patch : m_Patches)
    {
        r.Patches.emplace_back(patch.second->GetResource(localPos, cameraPatchXy));
    }

    return r;
}
