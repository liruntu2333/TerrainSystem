#pragma once

#include <directxtk/BufferHelpers.h>
#include "MeshVertex.h"
#include "Renderer.h"
#include "Patch.h"
#include "TerrainSystem.h"

class TINRenderer : public Renderer
{
public:
    TINRenderer(
        ID3D11Device* device,
        const std::shared_ptr<DirectX::ConstantBuffer<PassConstants>>& passConst);

    ~TINRenderer() override = default;

    TINRenderer(const TINRenderer&) = delete;
    TINRenderer& operator=(const TINRenderer&) = delete;
    TINRenderer(TINRenderer&&) = delete;
    TINRenderer& operator=(TINRenderer&&) = delete;

    void Initialize(ID3D11DeviceContext* context);
    void Render(ID3D11DeviceContext* context, const TerrainSystem::PatchRenderResource& r, bool wireFrame = false);

protected:
    struct ObjectConstants
    {
        DirectX::XMUINT2 PatchXy;
        uint32_t Color;
        int Padding;
    };

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_Vs = nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_Ps = nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_WireFramePs = nullptr;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_InputLayout = nullptr;

    DirectX::ConstantBuffer<ObjectConstants> m_Cb1 {};
    std::shared_ptr<DirectX::ConstantBuffer<PassConstants>> m_Cb0 = nullptr;
};
