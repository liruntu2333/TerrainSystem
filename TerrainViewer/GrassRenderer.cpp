#include "GrassRenderer.h"

#include <d3dcompiler.h>

using namespace DirectX;
using namespace SimpleMath;

namespace
{
    constexpr uint32_t MAX_GRASS_COUNT = 0x80000;
    constexpr uint32_t GROUP_SIZE_X    = 256;
    const uint16_t HIGH_LOD_INDEX[15]  = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 };
}

GrassRenderer::GrassRenderer(ID3D11Device* device) : Renderer(device), m_Cb0(device)
{
    {
        const CD3D11_BUFFER_DESC bufferDesc(
            5 * sizeof(uint32_t),
            D3D11_BIND_UNORDERED_ACCESS,
            D3D11_USAGE_DEFAULT,
            0,
            D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS,
            sizeof(uint32_t));

        ThrowIfFailed(device->CreateBuffer(&bufferDesc, nullptr, &m_IndirectDrawArg));

        const CD3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc(m_IndirectDrawArg.Get(), DXGI_FORMAT_R32_UINT, 0, 5, 0);
        ThrowIfFailed(device->CreateUnorderedAccessView(m_IndirectDrawArg.Get(), &uavDesc, &m_DrawArgUav));
    }

    {
        const CD3D11_BUFFER_DESC bufferDesc(
            MAX_GRASS_COUNT * sizeof(InstanceData),
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
            D3D11_USAGE_DEFAULT,
            0,
            D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
            sizeof(InstanceData));

        ThrowIfFailed(device->CreateBuffer(&bufferDesc, nullptr, &m_InstanceBuffer));

        const CD3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc(
            m_InstanceBuffer.Get(),
            DXGI_FORMAT_UNKNOWN,
            0,
            MAX_GRASS_COUNT,
            D3D11_BUFFER_UAV_FLAG_APPEND);
        ThrowIfFailed(device->CreateUnorderedAccessView(m_InstanceBuffer.Get(), &uavDesc, &m_InstanceUav));

        const CD3D11_SHADER_RESOURCE_VIEW_DESC srvDesc(
            m_InstanceBuffer.Get(),
            DXGI_FORMAT_UNKNOWN,
            0,
            MAX_GRASS_COUNT);
        ThrowIfFailed(device->CreateShaderResourceView(m_InstanceBuffer.Get(), &srvDesc, &m_InstanceSrv));
    }
    ThrowIfFailed(CreateStaticBuffer(device, HIGH_LOD_INDEX, _countof(HIGH_LOD_INDEX), D3D11_BIND_INDEX_BUFFER, &m_HighLodIb));
}

void GrassRenderer::Initialize(const std::filesystem::path& shaderDir)
{
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    auto name = (shaderDir / "GrassGeneration.cso").wstring();
    ThrowIfFailed(D3DReadFileToBlob(name.c_str(), &blob));
    ThrowIfFailed(
        m_Device->CreateComputeShader(
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            nullptr,
            &m_Cs)
        );

    name = (shaderDir / "GrassVS.cso").wstring();
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

void GrassRenderer::Render(
    ID3D11DeviceContext* context,
    const StructuredBuffer<BaseVertex>& baseVb,
    const StructuredBuffer<uint32_t>& baseIb,
    ID3D11ShaderResourceView* grassAlbedo,
    const Matrix& baseRotTrans, const Matrix& viewProj, const float baseArea, float density, bool wireFrame)
{
    density = std::clamp(density, 0.0f, MAX_GRASS_COUNT / baseArea);

    XMFLOAT3X3 invTransBaseWorld(
        baseRotTrans._11, baseRotTrans._12, baseRotTrans._13,
        baseRotTrans._21, baseRotTrans._22, baseRotTrans._23,
        baseRotTrans._31, baseRotTrans._32, baseRotTrans._33);
    XMMATRIX invTrans = XMLoadFloat3x3(&invTransBaseWorld);
    invTrans          = XMMatrixInverse(nullptr, invTrans);
    XMStoreFloat3x3(&invTransBaseWorld, invTrans);
    Uniforms uniforms;
    uniforms.ViewProj          = viewProj.Transpose();
    uniforms.BaseWorld         = baseRotTrans.Transpose();
    uniforms.InvTransBaseWorld = invTransBaseWorld;
    uniforms.GrassIdxCnt       = 15;
    uniforms.NumBaseTriangle   = baseIb.m_Capacity / 3;
    uniforms.InvSumBaseArea    = 1.0f / baseArea;
    uniforms.Density           = density;
    m_Cb0.SetData(context, uniforms);
    ID3D11Buffer* b0 = m_Cb0.GetBuffer();

    context->CSSetShader(m_Cs.Get(), nullptr, 0);
    context->CSSetConstantBuffers(0, 1, &b0);
    ID3D11ShaderResourceView* csSrv[] = { baseVb.GetSrv(), baseIb.GetSrv() };
    context->CSSetShaderResources(0, _countof(csSrv), csSrv);
    ID3D11UnorderedAccessView* csUav[] = { m_DrawArgUav.Get(), m_InstanceUav.Get() };
    constexpr UINT uavInit[]           = { 5, 0 };
    context->CSSetUnorderedAccessViews(0, _countof(csUav), csUav, uavInit);
    context->Dispatch((baseIb.m_Capacity / 3 + GROUP_SIZE_X - 1) / GROUP_SIZE_X, 1, 1);
    csUav[0] = csUav[1] = nullptr;
    context->CSSetUnorderedAccessViews(0, _countof(csUav), csUav, nullptr);

    constexpr UINT stride = sizeof(BaseVertex);
    constexpr UINT offset = 0;
    ID3D11Buffer* vb      = nullptr;
    context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    context->IASetIndexBuffer(m_HighLodIb.Get(), DXGI_FORMAT_R16_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    context->VSSetShader(m_Vs.Get(), nullptr, 0);
    context->VSSetConstantBuffers(0, 1, &b0);
    ID3D11ShaderResourceView* vsSrv[] = { m_InstanceSrv.Get() };
    context->VSSetShaderResources(0, _countof(vsSrv), vsSrv);

    context->RSSetState(wireFrame ? s_CommonStates->Wireframe() : s_CommonStates->CullNone());

    context->PSSetShader(m_Ps.Get(), nullptr, 0);
    context->PSSetShaderResources(0, 1, &grassAlbedo);
    auto* sampler = s_CommonStates->AnisotropicWrap();
    context->PSSetSamplers(0, 1, &sampler);

    context->OMSetBlendState(s_CommonStates->Opaque(), nullptr, 0xffffffff);
    context->OMSetDepthStencilState(s_CommonStates->DepthDefault(), 0);

    context->DrawIndexedInstancedIndirect(m_IndirectDrawArg.Get(), 0);

    vsSrv[0] = nullptr;
    context->VSSetShaderResources(0, _countof(vsSrv), vsSrv);
}
