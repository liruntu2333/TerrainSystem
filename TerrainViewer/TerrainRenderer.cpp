#define NOMINMAX

#include "TerrainRenderer.h"
#include <d3dcompiler.h>
#include "D3DHelper.h"

using namespace DirectX;

TerrainRenderer::TerrainRenderer(ID3D11Device* device, const std::shared_ptr<PassConstants>& constants) :
    Renderer(device), m_Cb0(device), m_Constants(constants) {}

void TerrainRenderer::Initialize(ID3D11DeviceContext* context)
{
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    ThrowIfFailed(D3DReadFileToBlob(L"./shader/MeshVS.cso", &blob));

    ThrowIfFailed(
        m_Device->CreateVertexShader(
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            nullptr,
            &m_Vs)
        );

    ThrowIfFailed(
        m_Device->CreateInputLayout(
            Vertex::InputElements,
            Vertex::InputElementCount,
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            &m_InputLayout)
        );

    ThrowIfFailed(D3DReadFileToBlob(L"./shader/MeshPS.cso", &blob));
    ThrowIfFailed(
        m_Device->CreatePixelShader(
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            nullptr,
            &m_Ps)
        );

    ThrowIfFailed(D3DReadFileToBlob(L"./shader/WireFramePS.cso", &blob));
    ThrowIfFailed(
        m_Device->CreatePixelShader(
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            nullptr,
            &m_WireFramePs)
        );
}

void TerrainRenderer::Render(
    ID3D11DeviceContext* context, const TerrainSystem::RenderResource& r, bool wireFrame)
{
    constexpr UINT stride = sizeof(Vertex);
    constexpr UINT offset = 0;
    context->IASetInputLayout(m_InputLayout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const auto cb0 = m_Cb0.GetBuffer();
    context->VSSetConstantBuffers(0, 1, &cb0);
    const auto pt = s_CommonStates->PointClamp();
    context->VSSetSamplers(0, 1, &pt);
    context->VSSetShader(m_Vs.Get(), nullptr, 0);
    const auto ani = s_CommonStates->AnisotropicClamp();
    context->PSSetSamplers(0, 1, &ani);
    context->PSSetConstantBuffers(0, 1, &cb0);
    context->OMSetBlendState(s_CommonStates->Opaque(), nullptr, 0xffffffff);
    context->OMSetDepthStencilState(s_CommonStates->DepthDefault(), 0);

    context->VSSetShaderResources(0, 1, &r.Height);
    context->PSSetShaderResources(0, 2, &r.Normal); // and albedo

    if (wireFrame)
    {
        context->RSSetState(s_CommonStates->Wireframe());
        context->PSSetShader(m_WireFramePs.Get(), nullptr, 0);
    }
    else
    {
        context->RSSetState(s_CommonStates->CullClockwise());
        context->PSSetShader(m_Ps.Get(), nullptr, 0);
    }

    for (const auto& patch : r.Patches)
    {
        m_Constants->Color = patch.Color;
        m_Constants->PatchOffset = patch.Offset;
        m_Constants->PatchXY = patch.PatchXY;
        UpdateBuffer(context);

        context->IASetVertexBuffers(0, 1, &patch.Vb, &stride, &offset);
        context->IASetIndexBuffer(patch.Ib, patch.Idx16Bit ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
        context->DrawIndexed(patch.IdxCnt, 0, 0);
    }
}

void TerrainRenderer::UpdateBuffer(ID3D11DeviceContext* context)
{
    m_Cb0.SetData(context, *m_Constants);
}
