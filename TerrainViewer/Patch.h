#pragma once

#include <array>
#include <d3d11.h>
#include <memory>
#include <directxtk/SimpleMath.h>
#include <wrl/client.h>
#include "Texture2D.h"

constexpr float PATCH_SIZE = 255.0f;

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

    [[nodiscard]] RenderResource GetResource(const DirectX::SimpleMath::Vector3& camera, const DirectX::XMINT2& cameraOffset) const;

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
