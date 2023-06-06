#pragma once

#include <map>
#include "Patch.h"

#include "Texture2D.h"
#include "../HeightMapSplitter/BoundTree.h"

class TerrainSystem
{
public:
    struct RenderResource
    {
        ID3D11ShaderResourceView* Height {};
        ID3D11ShaderResourceView* Normal {};
        ID3D11ShaderResourceView* Albedo {};
        std::vector<Patch::RenderResource> Patches {};

        RenderResource() = default;
    };

    TerrainSystem(std::filesystem::path path, ID3D11Device* device);
    ~TerrainSystem() = default;

    [[nodiscard]] RenderResource GetPatchResources(
        const DirectX::XMINT2& camXyForCull,
        const DirectX::BoundingFrustum& frustumLocal, std::vector<DirectX::BoundingBox>& bbs, ID3D11Device* device) const;

protected:
    std::map<int, std::shared_ptr<Patch>> m_Patches {};
    std::unique_ptr<BoundTree> m_BoundTree = nullptr;

    std::unique_ptr<DirectX::Texture2D> m_Height {};
    std::unique_ptr<DirectX::Texture2D> m_Normal {};
    std::unique_ptr<DirectX::Texture2D> m_Albedo {};

    const std::filesystem::path m_Path;
};
