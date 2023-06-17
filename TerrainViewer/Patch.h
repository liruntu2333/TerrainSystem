#pragma once

#include <d3d11.h>
#include <filesystem>
#include <future>
#include <directxtk/SimpleMath.h>
#include <wrl/client.h>

constexpr float PATCH_SIZE = 255.0f;

class Patch
{
public:
    Patch(const std::filesystem::path& path, int x, int y, ID3D11Device* device);
    ~Patch() = default;

    struct RenderResource
    {
        ID3D11Buffer* Vb;
        ID3D11Buffer* Ib;
        uint32_t IdxCnt;
        uint32_t Color;
        DirectX::XMUINT2 PatchXy; // Left bottom corner of the patch in global texture.
        bool Idx16Bit;
    };

    [[nodiscard]] RenderResource GetResource(const std::filesystem::path& path, int lod, ID3D11Device* device);

    [[nodiscard]] DirectX::SimpleMath::Vector3 GetLocalPosition(const DirectX::XMINT2& cameraOffset) const
    {
        return { (m_X - cameraOffset.x) * PATCH_SIZE, 0.0f, (m_Y - cameraOffset.y) * PATCH_SIZE };
    }

    friend class TerrainSystem;

private:
    struct LodResource
    {
        Microsoft::WRL::ComPtr<ID3D11Buffer> Vb {};
        Microsoft::WRL::ComPtr<ID3D11Buffer> Ib {};
        uint32_t IdxCnt {};
        bool Idx16Bit {};
    };

    std::shared_ptr<LodResource> LoadResource(const std::filesystem::path& path, int lod, ID3D11Device* device) const;

    const int m_X;
    const int m_Y;
    static constexpr int LOWEST_LOD = 2;
    int m_Lod = LOWEST_LOD;
    int m_LodStreaming = LOWEST_LOD;

    std::shared_ptr<LodResource> m_Resource {};
    std::future<std::shared_ptr<LodResource>> m_Stream {};
};
