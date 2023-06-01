#pragma once

#include "Patch.h"

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

    TerrainSystem(const std::filesystem::path& path, ID3D11Device* device);
    ~TerrainSystem() = default;

    [[nodiscard]] RenderResource GetPatchResources(
        const DirectX::SimpleMath::Vector3& worldPos, DirectX::SimpleMath::Vector3& localPos) const;

private:
    std::vector<std::shared_ptr<Patch>> m_Patches {};

    std::unique_ptr<DirectX::Texture2D> m_Height {};
    std::unique_ptr<DirectX::Texture2D> m_Normal {};
    std::unique_ptr<DirectX::Texture2D> m_Albedo {};
};