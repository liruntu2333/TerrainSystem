#pragma once

#include <filesystem>
#include <directxtk/BufferHelpers.h>
#include <wrl/client.h>

#include "Renderer.h"
#include "TerrainSystem.h"
#include "VertexBuffer.h"

class ClipmapRenderer : public Renderer
{
public:
    ClipmapRenderer(
        ID3D11Device* device, const std::shared_ptr<DirectX::ConstantBuffer<PassConstants>>& passConst);

    ~ClipmapRenderer() override = default;
    ClipmapRenderer(const ClipmapRenderer&) = delete;
    ClipmapRenderer& operator=(const ClipmapRenderer&) = delete;
    ClipmapRenderer(ClipmapRenderer&&) = delete;
    ClipmapRenderer& operator=(ClipmapRenderer&&) = delete;

    void Initialize();
    void Render(ID3D11DeviceContext* context, const TerrainSystem::ClipmapRenderResource& rr);

protected:

    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_InputLayout = nullptr;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_Vs = nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_Ps = nullptr;

    std::shared_ptr<DirectX::ConstantBuffer<PassConstants>> m_Cb0;
    DirectX::VertexBuffer<GridInstance> m_InsBuff;
};
