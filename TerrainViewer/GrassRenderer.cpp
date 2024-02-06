#include "GrassRenderer.h"

#include <array>
#include <d3dcompiler.h>

using namespace DirectX;
using namespace SimpleMath;

namespace
{
    constexpr uint32_t MAX_GRASS_COUNT = 0x800000;
    constexpr uint32_t GROUP_SIZE_X    = 256;
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

        ThrowIfFailed(device->CreateBuffer(&bufferDesc, nullptr, &m_InstData));

        const CD3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc(
            m_InstData.Get(),
            DXGI_FORMAT_UNKNOWN,
            0,
            MAX_GRASS_COUNT,
            D3D11_BUFFER_UAV_FLAG_APPEND);
        ThrowIfFailed(device->CreateUnorderedAccessView(m_InstData.Get(), &uavDesc, &m_InstUav));

        const CD3D11_SHADER_RESOURCE_VIEW_DESC srvDesc(
            m_InstData.Get(),
            DXGI_FORMAT_UNKNOWN,
            0,
            MAX_GRASS_COUNT);
        ThrowIfFailed(device->CreateShaderResourceView(m_InstData.Get(), &srvDesc, &m_InstSrv));
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
            &m_GenGrassCs)
        );

    //name = (shaderDir / "AssignSample.cso").wstring();
    //ThrowIfFailed(D3DReadFileToBlob(name.c_str(), &blob));
    //ThrowIfFailed(
    //    m_Device->CreateComputeShader(
    //        blob->GetBufferPointer(),
    //        blob->GetBufferSize(),
    //        nullptr,
    //        &m_AssignSamCs)
    //    );

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

void GrassRenderer::Render(
    ID3D11DeviceContext* context,
    const StructuredBuffer<BaseVertex>& baseVb,
    const StructuredBuffer<uint32_t>& baseIb,
    ID3D11ShaderResourceView* grassAlbedo,
    const Matrix& baseWorld, const Matrix& viewProj,
    const float baseArea, float density,
    const Vector2& height, const Vector2& width,
    const Vector2& bend, const Vector4& grav, const Vector4& wind, const DirectX::SimpleMath::Vector3& camPos,
    float orientThreshold, const std::array<Vector4, 6>& planes, bool wireFrame)
{
    density = std::clamp(density, 0.0f, MAX_GRASS_COUNT / baseArea);

    Uniforms uniforms;
    uniforms.ViewProj        = viewProj.Transpose();
    uniforms.BaseWorld       = baseWorld.Transpose();
    uniforms.NumBaseTriangle = baseIb.m_Capacity / 3;
    uniforms.Density         = density;
    uniforms.Height          = height;
    uniforms.Width           = width;
    uniforms.BendFactor      = bend;
    uniforms.Gravity         = grav;
    uniforms.Wind            = wind;
    uniforms.CamPos          = camPos;
    uniforms.OrientThreshold = orientThreshold;
    std::copy_n(planes.begin(), 6, uniforms.Planes);
    m_Cb0.SetData(context, uniforms);
    ID3D11Buffer* b0 = m_Cb0.GetBuffer();

    //{
    //    context->CSSetShader(m_AssignSamCs.Get(), nullptr, 0);
    //    context->CSSetConstantBuffers(0, 1, &b0);
    //    ID3D11ShaderResourceView* csSrv[] = { baseVb.GetSrv(), baseIb.GetSrv() };
    //    context->CSSetShaderResources(0, _countof(csSrv), csSrv);
    //    ID3D11UnorderedAccessView* csUav[] = { m_ArgUav.Get(), m_SamIdxUav.Get() };
    //    constexpr UINT uavInit[]           = { 10, 0 };
    //    context->CSSetUnorderedAccessViews(0, _countof(csUav), csUav, uavInit);
    //    context->Dispatch((baseIb.m_Capacity / 3 + GROUP_SIZE_X - 1) / GROUP_SIZE_X, 1, 1);
    //    csUav[0] = csUav[1] = nullptr;
    //    context->CSSetUnorderedAccessViews(0, _countof(csUav), csUav, nullptr);
    //}

    {
        context->CSSetShader(m_GenGrassCs.Get(), nullptr, 0);
        context->CSSetConstantBuffers(0, 1, &b0);
        ID3D11ShaderResourceView* csSrv[] = { baseVb.GetSrv(), baseIb.GetSrv() };
        context->CSSetShaderResources(0, _countof(csSrv), csSrv);
        ID3D11UnorderedAccessView* csUav[] = { m_ArgUav.Get(), m_InstUav.Get() };
        constexpr UINT uavInit[]           = { 10, 0 };
        context->CSSetUnorderedAccessViews(0, _countof(csUav), csUav, uavInit);
        //context->DispatchIndirect(m_IndirectArg.Get(), 0);
        context->Dispatch((baseIb.m_Capacity / 3 + GROUP_SIZE_X - 1) / GROUP_SIZE_X, 1, 1);
        csUav[0] = csUav[1] = nullptr;
        context->CSSetUnorderedAccessViews(0, _countof(csUav), csUav, nullptr);
        //csSrv[2] = nullptr;
        //context->CSSetShaderResources(2, 1, &csSrv[2]);
    }

    {
        constexpr UINT stride = sizeof(BaseVertex);
        constexpr UINT offset = 0;
        ID3D11Buffer* vb      = nullptr;
        context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        context->IASetIndexBuffer(m_HighLodIb.Get(), DXGI_FORMAT_R16_UINT, 0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        context->VSSetShader(m_Vs.Get(), nullptr, 0);
        context->VSSetConstantBuffers(0, 1, &b0);
        ID3D11ShaderResourceView* vsSrv[] = { m_InstSrv.Get() };
        context->VSSetShaderResources(0, _countof(vsSrv), vsSrv);

        context->RSSetState(wireFrame ? s_CommonStates->Wireframe() : s_CommonStates->CullNone());

        context->PSSetShader(m_Ps.Get(), nullptr, 0);
        context->PSSetShaderResources(0, 1, &grassAlbedo);
        auto* sampler = s_CommonStates->AnisotropicWrap();
        context->PSSetSamplers(0, 1, &sampler);

        context->OMSetBlendState(s_CommonStates->Opaque(), nullptr, 0xffffffff);
        context->OMSetDepthStencilState(s_CommonStates->DepthDefault(), 0);

        context->DrawIndexedInstancedIndirect(m_IndirectArg.Get(), sizeof(uint32_t) * 5);

        vsSrv[0] = nullptr;
        context->VSSetShaderResources(0, _countof(vsSrv), vsSrv);
    }
}
