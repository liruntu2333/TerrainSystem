#pragma once

#include <array>
#include <d3d11.h>
#include <filesystem>
#include <directxtk/SimpleMath.h>
#include <wrl/client.h>

constexpr float PATCH_SIZE = 255.0f;
constexpr float PATCH_HEIGHT_RANGE = 2000.0f;

class Patch
{
public:
    Patch(const std::filesystem::path& path, ID3D11Device* device);
    ~Patch() = default;

    struct RenderResource
    {
        ID3D11Buffer* Vb;
        ID3D11Buffer* Ib;
        uint32_t IdxCnt;
        uint32_t Color;
        DirectX::XMINT2 Offset;
        DirectX::XMUINT2 PatchXY; // Left bottom corner of the patch in global texture.
        bool Idx16Bit;
    };

    [[nodiscard]] RenderResource GetResource(
        const DirectX::SimpleMath::Vector3& camera, const DirectX::XMINT2& cameraOffset) const;

    [[nodiscard]] DirectX::SimpleMath::Vector3 GetLocalPosition(const DirectX::XMINT2& cameraOffset) const
    {
        return { (m_X - cameraOffset.x) * PATCH_SIZE, 0.0f, (-m_Y - cameraOffset.y) * PATCH_SIZE };
    }

    friend class TerrainSystem;

private:
    int m_X;
    int m_Y;

    struct LodResource
    {
        Microsoft::WRL::ComPtr<ID3D11Buffer> Vb {};
        Microsoft::WRL::ComPtr<ID3D11Buffer> Ib {};
        uint32_t IdxCnt {};
        bool Idx16Bit {};
    };

    std::array<LodResource, 5> m_Lods;
};
