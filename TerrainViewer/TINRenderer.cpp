#define NOMINMAX

#include "TINRenderer.h"
#include <d3dcompiler.h>
#include "D3DHelper.h"

using namespace DirectX;
using Vertex = MeshVertex;

TINRenderer::TINRenderer(
    ID3D11Device* device,
    const std::shared_ptr<ConstantBuffer<PassConstants>>& passConst) :
    Renderer(device), m_Cb1(device), m_Cb0(passConst) {}

void TINRenderer::Initialize(ID3D11DeviceContext* context, const std::filesystem::path& shaderDir)
{
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    auto name = (shaderDir / "MeshVS.cso").wstring();
    ThrowIfFailed(D3DReadFileToBlob(name.c_str(), &blob));

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

    name = (shaderDir / "MeshPS.cso").wstring();
    ThrowIfFailed(D3DReadFileToBlob(name.c_str(), &blob));
    ThrowIfFailed(
        m_Device->CreatePixelShader(
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            nullptr,
            &m_Ps)
        );

    name = (shaderDir / "WireFramePS.cso").wstring();
    ThrowIfFailed(D3DReadFileToBlob(name.c_str(), &blob));
    ThrowIfFailed(
        m_Device->CreatePixelShader(
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            nullptr,
            &m_WireFramePs)
        );
}

void TINRenderer::Render(
    ID3D11DeviceContext* context, const TerrainSystem::PatchRenderResource& r, bool wireFrame)
{
    constexpr UINT stride = sizeof(Vertex);
    constexpr UINT offset = 0;
    context->IASetInputLayout(m_InputLayout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11Buffer* cbs[] = { m_Cb0->GetBuffer(), m_Cb1.GetBuffer() };
    context->VSSetConstantBuffers(0, 2, &cbs[0]);
    const auto pt = s_CommonStates->PointClamp();
    context->VSSetSamplers(0, 1, &pt);
    context->VSSetShader(m_Vs.Get(), nullptr, 0);
    const auto ani = s_CommonStates->AnisotropicClamp();
    context->PSSetSamplers(0, 1, &ani);
    context->PSSetConstantBuffers(0, 2, &cbs[0]);
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
        context->RSSetState(s_CommonStates->CullCounterClockwise());
        context->PSSetShader(m_Ps.Get(), nullptr, 0);
    }

    for (const auto& patch : r.Patches)
    {
        ObjectConstants object;
        object.Color = patch.Color;
        object.PatchXy = patch.PatchXy;
        m_Cb1.SetData(context, object);

        context->IASetVertexBuffers(0, 1, &patch.Vb, &stride, &offset);
        context->IASetIndexBuffer(patch.Ib, patch.Idx16Bit ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
        context->DrawIndexed(patch.IdxCnt, 0, 0);
    }
}
