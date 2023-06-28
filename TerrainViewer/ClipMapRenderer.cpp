#define NOMINMAX

#include "ClipmapRenderer.h"
#include <d3dcompiler.h>
#include <directxtk/BufferHelpers.h>
#include "D3DHelper.h"
#include "Vertex.h"

using namespace DirectX;
using namespace SimpleMath;

ClipmapRenderer::ClipmapRenderer(
    ID3D11Device* device,
    const std::shared_ptr<ConstantBuffer<PassConstants>>& passConst) :
    Renderer(device), m_Cb0(passConst), m_InsBuff(m_Device,
        CD3D11_BUFFER_DESC(0, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE),
        TerrainSystem::LevelCount * 14 + 5) {}

void ClipmapRenderer::Initialize(const std::filesystem::path& shaderDir)
{
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    auto name = (shaderDir / "GridVS.cso").wstring();
    ThrowIfFailed(D3DReadFileToBlob(name.c_str(), &blob));

    ThrowIfFailed(
        m_Device->CreateVertexShader(
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            nullptr,
            &m_Vs)
        );

    std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
    elements.reserve(GridVertex::InputElementCount + GridInstance::InputElementCount);
    for (const auto& InputElement : GridVertex::InputElements)
        elements.push_back(InputElement);
    for (const auto& InputElement : GridInstance::InputElements)
        elements.push_back(InputElement);

    ThrowIfFailed(
        m_Device->CreateInputLayout(
            elements.data(),
            GridVertex::InputElementCount + GridInstance::InputElementCount,
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            &m_InputLayout)
        );

    name = shaderDir / "GridPS.cso";
    ThrowIfFailed(D3DReadFileToBlob(name.c_str(), &blob));
    ThrowIfFailed(
        m_Device->CreatePixelShader(
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            nullptr,
            &m_Ps)
        );

    name = shaderDir / "GridWireFrame.cso";
    ThrowIfFailed(D3DReadFileToBlob(name.c_str(), &blob));
    ThrowIfFailed(
        m_Device->CreatePixelShader(
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            nullptr,
            &m_WireFramePs)
        );
}

void ClipmapRenderer::Render(
    ID3D11DeviceContext* context, const TerrainSystem::ClipmapRenderResource& rr, bool wireFrame)
{
    m_InsBuff.SetData(context, rr.Grids.data(), rr.Grids.size());

    constexpr UINT stride = sizeof(GridVertex);
    constexpr UINT offset = 0;
    context->IASetInputLayout(m_InputLayout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const auto& ins = static_cast<ID3D11Buffer*>(m_InsBuff);
    {
        constexpr UINT instanceStride = sizeof(GridInstance);
        constexpr UINT instanceOffset = 0;
        context->IASetVertexBuffers(1, 1, &ins, &instanceStride, &instanceOffset);
    }

    ID3D11Buffer* cbs[] = { m_Cb0->GetBuffer() };
    context->VSSetConstantBuffers(0, _countof(cbs), &cbs[0]);
    const auto pw = s_CommonStates->PointWrap();
    context->VSSetSamplers(0, 1, &pw);
    context->VSSetShader(m_Vs.Get(), nullptr, 0);

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

    context->PSSetConstantBuffers(0, _countof(cbs), &cbs[0]);
    const auto lw = s_CommonStates->LinearWrap();
    context->PSSetSamplers(0, 1, &lw);
    context->OMSetBlendState(s_CommonStates->Opaque(), nullptr, 0xffffffff);
    context->OMSetDepthStencilState(s_CommonStates->DepthDefault(), 0);

    context->VSSetShaderResources(0, 1, &rr.HeightCm);
    context->PSSetShaderResources(0, 2, &rr.Normal); // and albedo

    const auto blkCnt = rr.RingInstanceStart - rr.BlockInstanceStart;
    if (blkCnt > 0)
    {
        const auto insStart = rr.BlockInstanceStart;
        context->IASetVertexBuffers(0, 1, &rr.BlockVb, &stride, &offset);
        context->IASetIndexBuffer(rr.BlockIb, DXGI_FORMAT_R16_UINT, 0);
        context->DrawIndexedInstanced(rr.BlockIdxCnt, blkCnt, 0, 0, insStart);
    }

    const auto ringCnt = rr.TrimInstanceStart[0] - rr.RingInstanceStart;
    if (ringCnt > 0)
    {
        const auto insStart = rr.RingInstanceStart;
        context->IASetVertexBuffers(0, 1, &rr.RingVb, &stride, &offset);
        context->IASetIndexBuffer(rr.RingIb, DXGI_FORMAT_R16_UINT, 0);
        context->DrawIndexedInstanced(rr.RingIdxCnt, ringCnt, 0, 0, insStart);
    }

    for (int i = 0; i < 4; ++i)
    {
        const auto insCnt = (i < 3 ? rr.TrimInstanceStart[i + 1] : rr.Grids.size()) - rr.TrimInstanceStart[i];
        if (insCnt == 0) continue;
        const auto insStart = rr.TrimInstanceStart[i];
        context->IASetVertexBuffers(0, 1, &rr.TrimVb[i], &stride, &offset);
        context->IASetIndexBuffer(rr.TrimIb[i], DXGI_FORMAT_R16_UINT, 0);
        context->DrawIndexedInstanced(rr.TrimIdxCnt, insCnt, 0, 0, insStart);
    }
}
