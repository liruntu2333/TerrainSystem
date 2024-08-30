#include "CompressedTerrainRenderer.h"

#include <d3dcompiler.h>

#include "D3DHelper.h"

using namespace DirectX;


constexpr int Res = 512;

void CompressedTerrainRenderer::Initialize(ID3D11DeviceContext* context, const std::filesystem::path& shaderDir)
{
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    auto name = (shaderDir / "TerrainOriginVS.cso").wstring();
    ThrowIfFailed(D3DReadFileToBlob(name.c_str(), &blob));

    name = (shaderDir / "TerrainCompressedVS.cso").wstring();
    ThrowIfFailed(D3DReadFileToBlob(name.c_str(), &blob));
    ThrowIfFailed(
        m_Device->CreateVertexShader(
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            nullptr,
            &m_Vs)
        );

    name = (shaderDir / "TerrainWaveletPS.cso").wstring();
    ThrowIfFailed(D3DReadFileToBlob(name.c_str(), &blob));
    ThrowIfFailed(
        m_Device->CreatePixelShader(
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            nullptr,
            &m_Ps)
        );

    std::vector<uint32_t> ib;
    for (int i = 0; i < Res - 1; ++i)
    {
        for (int j = 0; j < Res - 1; ++j)
        {
            ib.push_back(i * Res + j);
            ib.push_back(i * Res + j + 1);
            ib.push_back((i + 1) * Res + j + 1);

            ib.push_back(i * Res + j);
            ib.push_back((i + 1) * Res + j + 1);
            ib.push_back((i + 1) * Res + j);
        }
    }
    ThrowIfFailed(CreateStaticBuffer(m_Device, ib, D3D11_BIND_INDEX_BUFFER, &m_Ib));

    m_Cb0.Create(m_Device);
}

void CompressedTerrainRenderer::Render(ID3D11DeviceContext* context,
                                       ID3D11ShaderResourceView* origin,
                                       ID3D11ShaderResourceView* compressed,
                                       const SimpleMath::Matrix& viewProj,
                                       float ratio, float maxError, bool wireFrame)
{
    bool isOrigin = origin == compressed;
    const Constants constants
    {
        viewProj.Transpose(),
        ratio,
        isOrigin ? SimpleMath::Vector3(0, 0, 1) : SimpleMath::Vector3(0, 1, 0),
        1.0f / maxError,
        {}
    };
    m_Cb0.SetData(context, constants);

    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetIndexBuffer(m_Ib.Get(), DXGI_FORMAT_R32_UINT, 0);
    ID3D11Buffer* cbs[] = { m_Cb0.GetBuffer() };
    context->VSSetConstantBuffers(0, 1, &cbs[0]);
    context->VSSetShader(m_Vs.Get(), nullptr, 0);

    ID3D11ShaderResourceView* srvs[] = { origin, compressed };
    context->VSSetShaderResources(0, 2, srvs);
    context->PSSetConstantBuffers(0, 1, &cbs[0]);
    context->PSSetShader(m_Ps.Get(), nullptr, 0);

    if (wireFrame)
    {
        context->RSSetState(s_CommonStates->Wireframe());
    }
    else
    {
        context->RSSetState(s_CommonStates->CullCounterClockwise());
    }
    context->OMSetBlendState(isOrigin ? s_CommonStates->AlphaBlend() : s_CommonStates->Opaque(), nullptr, 0xffffffff);
    context->OMSetDepthStencilState(isOrigin ? s_CommonStates->DepthReadReverseZ() : s_CommonStates->DepthReverseZ(), 0);
    context->DrawIndexed((Res - 1) * (Res - 1) * 6, 0, 0);
}
