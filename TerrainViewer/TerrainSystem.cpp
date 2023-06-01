#include "TerrainSystem.h"

#include <iostream>
#include <string>

#include "../HeightMapSplitter/ThreadPool.h"

constexpr size_t PATCH_NY = 32;
constexpr size_t PATCH_NX = 32;

TerrainSystem::TerrainSystem(const std::filesystem::path& path, ID3D11Device* device)
{
    std::vector<std::future<std::shared_ptr<Patch>>> results;
    for (int x = 0; x < PATCH_NX; ++x)
        for (int y = 0; y < PATCH_NY; ++y)
            results.emplace_back(g_ThreadPool.enqueue([&path, device, this, x, y]
            {
                std::filesystem::path patchPath = path.u8string() + '/' + std::to_string(x) + "_" +
                    std::to_string(y);
                auto patch = std::make_shared<Patch>(patchPath, device);
                std::cout << "Loaded patch " << x << ", " << y << std::endl;
                return patch;
            }));

    for (auto& result : results)
        m_Patches.emplace_back(result.get());

    m_Height = std::make_unique<DirectX::Texture2D>(device, path.string() + "/height.dds");
    m_Height->CreateViews(device);
    m_Normal = std::make_unique<DirectX::Texture2D>(device, path.string() + "/normal.dds");
    m_Normal->CreateViews(device);
    m_Albedo = std::make_unique<DirectX::Texture2D>(device, path.string() + "/albedo.png");
    m_Albedo->CreateViews(device);
}

TerrainSystem::RenderResource TerrainSystem::GetPatchResources(
    const DirectX::SimpleMath::Vector3& worldPos, DirectX::SimpleMath::Vector3& localPos) const
{
    const DirectX::XMINT2 cameraPatch =
    {
        static_cast<int>(worldPos.x / PATCH_SIZE),
        static_cast<int>(worldPos.z / PATCH_SIZE)
    };

    localPos = worldPos - DirectX::SimpleMath::Vector3(cameraPatch.x * PATCH_SIZE, 0.0f, cameraPatch.y * PATCH_SIZE);

    RenderResource r;
    r.Height = m_Height->GetSrv();
    r.Normal = m_Normal->GetSrv();
    r.Albedo = m_Albedo->GetSrv();
    for (const auto& patch : m_Patches)
        r.Patches.emplace_back(patch->GetResource(localPos, cameraPatch));

    return r;
}
