#include "GrassRenderer.h"

#include <array>
#include <d3dcompiler.h>

using namespace DirectX;
using namespace SimpleMath;

namespace
{
    constexpr uint32_t MAX_GRASS_COUNT = 0x400000;
    const uint16_t HIGH_LOD_INDEX[15]  = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 };
}

GrassRenderer::GrassRenderer(ID3D11Device* device) : Renderer(device), m_Cb0(device)
{
    {
        const CD3D11_BUFFER_DESC bufferDesc(
            10 * sizeof(uint32_t),
            D3D11_BIND_UNORDERED_ACCESS,
            D3D11_USAGE_DEFAULT,
            0,
            D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS,
            sizeof(uint32_t));

        ThrowIfFailed(device->CreateBuffer(&bufferDesc, nullptr, &m_IndirectArg));

        CD3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc(m_IndirectArg.Get(), DXGI_FORMAT_R32_UINT, 0, 10, 0);
        ThrowIfFailed(device->CreateUnorderedAccessView(m_IndirectArg.Get(), &uavDesc, &m_ArgUav));
    }

    {
        const CD3D11_BUFFER_DESC bufferDesc(
            MAX_GRASS_COUNT * sizeof(InstanceData),
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
            D3D11_USAGE_DEFAULT,
            0,
            D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
            sizeof(InstanceData));

        ThrowIfFailed(device->CreateBuffer(&bufferDesc, nullptr, &m_Lod0Inst));
        ThrowIfFailed(device->CreateBuffer(&bufferDesc, nullptr, &m_Lod1Inst));

        const CD3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc(
            static_cast<ID3D11Buffer*>(nullptr),
            DXGI_FORMAT_UNKNOWN,
            0,
            MAX_GRASS_COUNT,
            D3D11_BUFFER_UAV_FLAG_APPEND);
        ThrowIfFailed(device->CreateUnorderedAccessView(m_Lod0Inst.Get(), &uavDesc, &m_Lod0Uav));
        ThrowIfFailed(device->CreateUnorderedAccessView(m_Lod1Inst.Get(), &uavDesc, &m_Lod1Uav));

        const CD3D11_SHADER_RESOURCE_VIEW_DESC srvDesc(
            static_cast<ID3D11Buffer*>(nullptr),
            DXGI_FORMAT_UNKNOWN,
            0,
            MAX_GRASS_COUNT);
        ThrowIfFailed(device->CreateShaderResourceView(m_Lod0Inst.Get(), &srvDesc, &m_Lod0Srv));
        ThrowIfFailed(device->CreateShaderResourceView(m_Lod1Inst.Get(), &srvDesc, &m_Lod1Srv));
    }

    {
        const CD3D11_BUFFER_DESC bufferDesc(
            MAX_GRASS_COUNT * sizeof(uint32_t),
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
            D3D11_USAGE_DEFAULT,
            0,
            D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
            sizeof(uint32_t));

        ThrowIfFailed(device->CreateBuffer(&bufferDesc, nullptr, &m_SamIdx));

        const CD3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc(
            m_SamIdx.Get(),
            DXGI_FORMAT_UNKNOWN,
            0,
            MAX_GRASS_COUNT,
            D3D11_BUFFER_UAV_FLAG_APPEND);
        ThrowIfFailed(device->CreateUnorderedAccessView(m_SamIdx.Get(), &uavDesc, &m_SamIdxUav));

        const CD3D11_SHADER_RESOURCE_VIEW_DESC srvDesc(
            m_SamIdx.Get(),
            DXGI_FORMAT_UNKNOWN,
            0,
            MAX_GRASS_COUNT);
        ThrowIfFailed(device->CreateShaderResourceView(m_SamIdx.Get(), &srvDesc, &m_SamIdxSrv));
    }

    ThrowIfFailed(CreateStaticBuffer(device, HIGH_LOD_INDEX, _countof(HIGH_LOD_INDEX), D3D11_BIND_INDEX_BUFFER,
        &m_HighLodIb));
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
            &m_GenGrassCs)
    );

    name = (shaderDir / "GenDepthMip3.cso").wstring();
    ThrowIfFailed(D3DReadFileToBlob(name.c_str(), &blob));
    ThrowIfFailed(
        m_Device->CreateComputeShader(
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            nullptr,
            &m_GenDepthMip3Cs)
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

    name = shaderDir / "GrassPS.cso";
    ThrowIfFailed(D3DReadFileToBlob(name.c_str(), &blob));
    ThrowIfFailed(
        m_Device->CreatePixelShader(
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            nullptr,
            &m_Ps)
    );
}

void GrassRenderer::GenerateHiZ3(
    ID3D11DeviceContext* context,
    ID3D11ShaderResourceView* depth0,
    ID3D11UnorderedAccessView* depthMip1,
    ID3D11UnorderedAccessView* depthMip2,
    ID3D11UnorderedAccessView* depthMip3, uint32_t rtW, uint32_t rtH)
{
    ID3D11UnorderedAccessView* uavs[] = { depthMip1, depthMip2, depthMip3 };
    context->CSSetShader(m_GenDepthMip3Cs.Get(), nullptr, 0);
    context->CSSetUnorderedAccessViews(0, _countof(uavs), uavs, nullptr);
    context->CSSetShaderResources(0, 1, &depth0);
    context->Dispatch(rtW + 7 / 8, rtH + 7 / 8, 1);
    context->CSSetUnorderedAccessViews(0, _countof(uavs), nullptr, nullptr);
    depth0 = nullptr;
    context->CSSetShaderResources(0, 1, &depth0);
    uavs[0] = uavs[1] = uavs[2] = uavs[3] = nullptr;
    context->CSSetUnorderedAccessViews(0, _countof(uavs), uavs, nullptr);
}

void GrassRenderer::GenerateInstance(
    ID3D11DeviceContext* context,
    const StructuredBuffer<VertexPositionNormalTexture>& baseVb,
    const StructuredBuffer<unsigned>& baseIb,
    ID3D11ShaderResourceView* depthMip,
    Uniforms& uniforms)
{
    const uint32_t triCnt = baseIb.m_Capacity / 3;
    //const uint32_t groupCntX = sqrt(triCnt);
    //const uint32_t groupCntY = (triCnt + groupCntX - 1) / groupCntX;
    const uint32_t groupCntX = sqrt(triCnt);
    const uint32_t groupCntY = (triCnt + groupCntX - 1) / groupCntX;

    uniforms.GroupCntX = groupCntX;
    uniforms.GroupCntY = groupCntY;

    m_Cb0.SetData(context, uniforms);
    ID3D11Buffer* b0 = m_Cb0.GetBuffer();

    context->CSSetShader(m_GenGrassCs.Get(), nullptr, 0);
    context->CSSetConstantBuffers(0, 1, &b0);
    ID3D11ShaderResourceView* csSrv[] = { baseVb.GetSrv(), baseIb.GetSrv(), depthMip };
    context->CSSetShaderResources(0, _countof(csSrv), csSrv);
    ID3D11UnorderedAccessView* csUav[] = { m_ArgUav.Get(), m_Lod0Uav.Get(), m_Lod1Uav.Get() };
    constexpr UINT uavInit[]           = { 10, 0, 0 };
    context->CSSetUnorderedAccessViews(0, _countof(csUav), csUav, uavInit);
    //context->DispatchIndirect(m_IndirectArg.Get(), 0);
    context->Dispatch(groupCntX, groupCntY, 1);
    std::memset(csUav, 0, sizeof(csUav));
    context->CSSetUnorderedAccessViews(0, _countof(csUav), csUav, nullptr);
    csSrv[2] = nullptr;
    context->CSSetShaderResources(2, 1, &csSrv[2]);
}

void GrassRenderer::Render(
    ID3D11DeviceContext* context,
    ID3D11ShaderResourceView* grassAlbedo,
    bool wireFrame)
{
    ID3D11Buffer* b0 = m_Cb0.GetBuffer();

    {
        constexpr UINT stride = sizeof(BaseVertex);
        constexpr UINT offset = 0;
        ID3D11Buffer* vb      = nullptr;
        context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        context->IASetIndexBuffer(m_HighLodIb.Get(), DXGI_FORMAT_R16_UINT, 0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        context->VSSetShader(m_Vs.Get(), nullptr, 0);
        context->VSSetConstantBuffers(0, 1, &b0);
        ID3D11ShaderResourceView* vsSrv[] = { m_Lod0Srv.Get() };
        context->VSSetShaderResources(0, _countof(vsSrv), vsSrv);

        context->RSSetState(wireFrame ? s_CommonStates->Wireframe() : s_CommonStates->CullNone());

        context->PSSetShader(m_Ps.Get(), nullptr, 0);
        context->PSSetConstantBuffers(0, 1, &b0);
        context->PSSetShaderResources(0, 1, &grassAlbedo);
        auto* sampler = s_CommonStates->AnisotropicWrap();
        context->PSSetSamplers(0, 1, &sampler);

        context->OMSetBlendState(s_CommonStates->Opaque(), nullptr, 0xffffffff);
        context->OMSetDepthStencilState(s_CommonStates->DepthDefault(), 0);

        context->DrawIndexedInstancedIndirect(m_IndirectArg.Get(), sizeof(uint32_t) * 0);

        vsSrv[0] = m_Lod1Srv.Get();
        context->VSSetShaderResources(0, _countof(vsSrv), vsSrv);
        context->DrawIndexedInstancedIndirect(m_IndirectArg.Get(), sizeof(uint32_t) * 5);

        vsSrv[0] = nullptr;
        context->VSSetShaderResources(0, _countof(vsSrv), vsSrv);
    }
}
