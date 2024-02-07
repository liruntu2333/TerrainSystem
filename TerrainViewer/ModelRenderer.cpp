#include "ModelRenderer.h"

#include <d3dcompiler.h>
#include <directxtk/VertexTypes.h>

using namespace DirectX;
using Vertex = VertexPositionNormalTexture;

ModelRenderer::ModelRenderer(ID3D11Device* device) : Renderer(device)
{
    m_Cb0.Create(device);
}

void ModelRenderer::Initialize(const std::filesystem::path& shaderDir)
{
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    auto name = (shaderDir / "ModelVS.cso").wstring();
    ThrowIfFailed(D3DReadFileToBlob(name.c_str(), &blob));

    ThrowIfFailed(
        m_Device->CreateVertexShader(
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            nullptr,
            &m_Vs)
        );

    name = shaderDir / "ModelPS.cso";
    ThrowIfFailed(D3DReadFileToBlob(name.c_str(), &blob));
    ThrowIfFailed(
        m_Device->CreatePixelShader(
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            nullptr,
            &m_Ps)
        );
}

void ModelRenderer::Render(
    ID3D11DeviceContext* context,
    const SimpleMath::Matrix& world,
    const SimpleMath::Matrix& viewProj,
    const StructuredBuffer<VertexPositionNormalTexture>& vb,
    const StructuredBuffer<uint32_t>& ib,
    const Texture2D& albedo, const DirectX::SimpleMath::Vector3& camPos, bool wireFrame)
{
    const Uniforms uniforms
    {
        (world * viewProj).Transpose(),
        viewProj.Transpose(),
        world.Transpose(),
        camPos
    };
    m_Cb0.SetData(context, uniforms);

    constexpr UINT stride = sizeof(Vertex);
    constexpr UINT offset = 0;
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->IASetVertexBuffers(0, 0, nullptr, &stride, &offset);
    context->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);

    ID3D11ShaderResourceView* geoSrvs[] = { vb.GetSrv(), ib.GetSrv() };
    ID3D11Buffer* cbs[]                 = { m_Cb0.GetBuffer() };
    context->VSSetConstantBuffers(0, _countof(cbs), cbs);
    context->VSSetShaderResources(0, _countof(geoSrvs), geoSrvs);
    context->VSSetShader(m_Vs.Get(), nullptr, 0);

    const auto ani = s_CommonStates->AnisotropicClamp();
    context->PSSetSamplers(0, 1, &ani);
    ID3D11ShaderResourceView* const albSrv = albedo.GetSrv();
    context->PSSetShaderResources(0, 1, &albSrv);
    context->PSSetConstantBuffers(0, _countof(cbs), cbs);
    context->RSSetState(wireFrame ? s_CommonStates->Wireframe() : s_CommonStates->CullClockwise());
    context->PSSetShader(m_Ps.Get(), nullptr, 0);

    context->OMSetBlendState(s_CommonStates->Opaque(), nullptr, 0xffffffff);
    context->OMSetDepthStencilState(s_CommonStates->DepthDefault(), 0);

    context->Draw(ib.m_Capacity, 0);
}
