#pragma once

#include <directxtk/BufferHelpers.h>
#include "TerrainVertex.h"
#include "Renderer.h"
#include "Patch.h"
#include "TerrainSystem.h"

using Vertex = TerrainVertex;

class TerrainRenderer : public Renderer
{
public:
    TerrainRenderer(ID3D11Device* device, const std::shared_ptr<PassConstants>& constants);
    ~TerrainRenderer() override = default;

    TerrainRenderer(const TerrainRenderer&) = delete;
    TerrainRenderer& operator=(const TerrainRenderer&) = delete;
    TerrainRenderer(TerrainRenderer&&) = delete;
    TerrainRenderer& operator=(TerrainRenderer&&) = delete;

    void Initialize(ID3D11DeviceContext* context);
    void Render(ID3D11DeviceContext* context, const TerrainSystem::RenderResource& r, bool wireFrame = false);

protected:

    void UpdateBuffer(ID3D11DeviceContext* context);

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_Vs = nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_Ps = nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_WireFramePs = nullptr;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_InputLayout = nullptr;
    DirectX::ConstantBuffer<PassConstants> m_Cb0 {};

    std::shared_ptr<PassConstants> m_Constants = nullptr;;
};
