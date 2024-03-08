#include "PlanetRenderer.h"

#include <d3dcompiler.h>
#include <DirectXColors.h>
#include <directxtk/BufferHelpers.h>

#include "D3DHelper.h"

using namespace DirectX;
using namespace SimpleMath;

namespace
{
    using Vertex = PlanetRenderer::Vertex;

    enum CubeFace
    {
        POSITIVE_X,
        NEGATIVE_X,
        POSITIVE_Y,
        NEGATIVE_Y,
        POSITIVE_Z,
        NEGATIVE_Z
    };

    constexpr Vector3 upAxis[] =
    {
        Vector3(+1, 0, 0),
        Vector3(-1, 0, 0),
        Vector3(0, +1, 0),
        Vector3(0, -1, 0),
        Vector3(0, 0, +1),
        Vector3(0, 0, -1)
    };

    constexpr Vector3 rhtAxis[] =
    {
        Vector3(0, 0, -1),
        Vector3(0, 0, +1),
        Vector3(+1, 0, 0),
        Vector3(+1, 0, 0),
        Vector3(+1, 0, 0),
        Vector3(-1, 0, 0)
    };

    constexpr Vector3 btmAxis[] =
    {
        Vector3(0, -1, 0),
        Vector3(0, -1, 0),
        Vector3(0, 0, +1),
        Vector3(0, 0, -1),
        Vector3(0, -1, 0),
        Vector3(0, -1, 0)
    };

    Vector3 GetCubeVertex(CubeFace face, float u, float v)
    {
        // switch (face)
        // {
        // case POSITIVE_X:
        //     vertex.position = Vector3(1.0f, 1.0f - 2.0f * v, 1.0f - 2.0f * u);
        //     break;
        // case NEGATIVE_X:
        //     vertex.position = Vector3(-1.0f, 1.0f - 2.0f * v, -1.0f + 2.0f * u);
        //     break;
        // case POSITIVE_Y:
        //     vertex.position = Vector3(-1.0f + 2.0f * u, 1.0f, -1.0f + 2.0f * v);
        //     break;
        // case NEGATIVE_Y:
        //     vertex.position = Vector3(-1.0f + 2.0f * u, -1.0f, 1.0f - 2.0f * v);
        //     break;
        // case POSITIVE_Z:
        //     vertex.position = Vector3(-1.0f + 2.0f * u, 1.0f - 2.0f * v, 1.0f);
        //     break;
        // case NEGATIVE_Z:
        //     vertex.position = Vector3(1.0f - 2.0f * u, 1.0f - 2.0f * v, -1.0f);
        //     break;
        // }

        return upAxis[face] + rhtAxis[face] * (u * 2.0f - 1.0f) + btmAxis[face] * (v * 2.0f - 1.0f);
    }
}

void PlanetRenderer::Initialize(const std::filesystem::path& shaderDir, int sphereTess)
{
    CreateSphere(sphereTess);
    CreateTexture();

    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    auto name = (shaderDir / "PlanetVS.cso").wstring();
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
            &m_VertexLayout)
        );

    name = shaderDir / "PlanetPS.cso";
    ThrowIfFailed(D3DReadFileToBlob(name.c_str(), &blob));
    ThrowIfFailed(
        m_Device->CreatePixelShader(
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            nullptr,
            &m_Ps)
        );

    m_Cb0.Create(m_Device);
}

void PlanetRenderer::Render(ID3D11DeviceContext* context, Uniforms uniforms, bool wireFrame)
{
    const auto cb = m_Cb0.GetBuffer();

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetInputLayout(m_VertexLayout.Get());

    constexpr uint32_t stride = sizeof(Vertex), offset = 0;
    context->IASetIndexBuffer(m_IndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    context->VSSetShader(m_Vs.Get(), nullptr, 0);
    context->VSSetConstantBuffers(0, 1, &cb);
    context->RSSetState(wireFrame ? s_CommonStates->Wireframe() : s_CommonStates->CullClockwise());

    context->PSSetShader(m_Ps.Get(), nullptr, 0);
    context->PSSetConstantBuffers(0, 1, &cb);
    ID3D11ShaderResourceView* srv[] = { m_AlbedoRoughnessSrv.Get(), m_F0MetallicSrv.Get() };
    context->PSSetShaderResources(0, _countof(srv), srv);
    ID3D11SamplerState* sam[] = { s_CommonStates->LinearClamp() };
    context->PSSetSamplers(0, _countof(sam), sam);

    context->OMSetDepthStencilState(s_CommonStates->DepthReverseZ(), 0);
    context->OMSetBlendState(s_CommonStates->Opaque(), nullptr, 0xFFFFFFFF);

    for (int i = 0; i < 6; ++i)
    {
        uniforms.debugColor = i == 0
                                  ? Colors::Red
                                  : i == 1
                                        ? Colors::Green
                                        : i == 2
                                              ? Colors::Blue
                                              : i == 3
                                                    ? Colors::Yellow
                                                    : i == 4
                                                          ? Colors::Cyan
                                                          : Colors::Magenta;

        m_Cb0.SetData(context, uniforms);
        const auto vb = m_VertexBuffers[i].Get();
        context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        context->DrawIndexed(m_IndicesPerFace, 0, 0);
    }
}

void PlanetRenderer::CreateSphere(uint16_t tesselation)
{
    std::vector<uint16_t> indices;
    const uint16_t grids = tesselation - 1;

    for (int i = 0; i < 6; ++i)
    {
        std::vector<Vertex> vertices;
        vertices.reserve(tesselation * tesselation);
        for (uint16_t j = 0; j < tesselation; ++j)
        {
            for (uint16_t k = 0; k < tesselation; ++k)
            {
                const float u = static_cast<float>(k) / static_cast<float>(grids);
                const float v = static_cast<float>(j) / static_cast<float>(grids);
                Vector3 pos   = GetCubeVertex(static_cast<CubeFace>(i), u, v);
                pos.Normalize();
                vertices.emplace_back(pos);
            }
        }
        ThrowIfFailed(CreateStaticBuffer(m_Device, vertices, D3D11_BIND_VERTEX_BUFFER, &m_VertexBuffers[i]));
    }

    indices.reserve(6 * grids * grids);
    for (uint16_t j = 0; j < grids; ++j)
    {
        for (uint16_t k = 0; k < grids; ++k)
        {
            const uint16_t i0 = j * tesselation + k;
            const uint16_t i1 = (j + 1) * tesselation + k;
            const uint16_t i2 = j * tesselation + k + 1;
            const uint16_t i3 = (j + 1) * tesselation + k + 1;

            indices.emplace_back(i0);
            indices.emplace_back(i1);
            indices.emplace_back(i2);

            indices.emplace_back(i2);
            indices.emplace_back(i1);
            indices.emplace_back(i3);
        }
    }

    ThrowIfFailed(CreateStaticBuffer(m_Device, indices, D3D11_BIND_INDEX_BUFFER, &m_IndexBuffer));
    m_IndicesPerFace = indices.size();
}

void PlanetRenderer::CreateTexture()
{
    std::vector<PackedVector::XMCOLOR> albRough;
    albRough.emplace_back(Colors::DarkBlue.f[0], Colors::DarkBlue.f[1], Colors::DarkBlue.f[2], 0.02f);
    albRough.emplace_back(Colors::DeepSkyBlue.f[0], Colors::DeepSkyBlue.f[1], Colors::DeepSkyBlue.f[2], 0.1f);
    albRough.emplace_back(Colors::Yellow.f[0], Colors::Yellow.f[1], Colors::Yellow.f[2], 0.05f);
    albRough.emplace_back(Colors::Gold.f[0], Colors::Gold.f[1], Colors::Gold.f[2], 0.1f);
    albRough.emplace_back(Colors::SaddleBrown.f[0], Colors::SaddleBrown.f[1], Colors::SaddleBrown.f[2], 0.2f);
    albRough.emplace_back(Colors::RosyBrown.f[0], Colors::RosyBrown.f[1], Colors::RosyBrown.f[2], 0.4f);
    albRough.emplace_back(Colors::ForestGreen.f[0], Colors::ForestGreen.f[1], Colors::ForestGreen.f[2], 0.6f);
    albRough.emplace_back(Colors::DarkOliveGreen.f[0], Colors::DarkOliveGreen.f[1], Colors::DarkOliveGreen.f[2], 0.3f);
    albRough.emplace_back(Colors::PaleGreen.f[0], Colors::PaleGreen.f[1], Colors::PaleGreen.f[2], 0.6f);
    albRough.emplace_back(Colors::DarkGray.f[0], Colors::DarkGray.f[1], Colors::DarkGray.f[2], 0.8f);
    albRough.emplace_back(Colors::DarkSlateGray.f[0], Colors::DarkSlateGray.f[1], Colors::DarkSlateGray.f[2], 0.8f);
    albRough.emplace_back(Colors::White.f[0], Colors::White.f[1], Colors::White.f[2], 0.1f);

    D3D11_SUBRESOURCE_DATA initData;
    initData.pSysMem          = albRough.data();
    initData.SysMemPitch      = 0;
    initData.SysMemSlicePitch = 0;

    const CD3D11_TEXTURE1D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM, albRough.size(), 1, 1);
    ThrowIfFailed(
        m_Device->CreateTexture1D(
            &desc,
            &initData,
            &m_AlbedoRoughness)
        );

    const CD3D11_SHADER_RESOURCE_VIEW_DESC srvDesc(m_AlbedoRoughness.Get(), D3D11_SRV_DIMENSION_TEXTURE1D);
    ThrowIfFailed(
        m_Device->CreateShaderResourceView(
            m_AlbedoRoughness.Get(),
            &srvDesc,
            &m_AlbedoRoughnessSrv)
        );

    std::vector<PackedVector::XMCOLOR> f0Metallic;
    f0Metallic.emplace_back(0.04f, 0.04f, 0.04f, 0.0f);
    f0Metallic.emplace_back(Colors::White.f[0], Colors::White.f[1], Colors::White.f[2], 0.31233f);
    f0Metallic.emplace_back(Colors::DarkRed.f[0], Colors::DarkRed.f[1], Colors::DarkRed.f[2], 0.82332f);
    f0Metallic.emplace_back(Colors::DarkGreen.f[0], Colors::DarkGreen.f[1], Colors::DarkGreen.f[2], 0.32332);
    f0Metallic.emplace_back(Colors::SteelBlue.f[0], Colors::SteelBlue.f[1], Colors::SteelBlue.f[2], 0.43232f);
    f0Metallic.emplace_back(Colors::DarkBlue.f[0], Colors::DarkBlue.f[1], Colors::DarkBlue.f[2], 0.1232f);
    f0Metallic.emplace_back(Colors::DarkOrange.f[0], Colors::DarkOrange.f[1], Colors::DarkOrange.f[2], 0.5332f);
    f0Metallic.emplace_back(Colors::DarkViolet.f[0], Colors::DarkViolet.f[1], Colors::DarkViolet.f[2], 0.322332f);
    f0Metallic.emplace_back(Colors::DarkCyan.f[0], Colors::DarkCyan.f[1], Colors::DarkCyan.f[2], 0.56565f);
    f0Metallic.emplace_back(Colors::DarkGray.f[0], Colors::DarkGray.f[1], Colors::DarkGray.f[2], 1.0f);
    f0Metallic.emplace_back(Colors::DarkSlateGray.f[0], Colors::DarkSlateGray.f[1], Colors::DarkSlateGray.f[2], 0.2323f);
    f0Metallic.emplace_back(Colors::LavenderBlush.f[0], Colors::LavenderBlush.f[1], Colors::LavenderBlush.f[2], 0.0f);

    initData.pSysMem = f0Metallic.data();
    const CD3D11_TEXTURE1D_DESC desc2(DXGI_FORMAT_B8G8R8A8_UNORM, f0Metallic.size(), 1, 1);
    ThrowIfFailed(
        m_Device->CreateTexture1D(
            &desc2,
            &initData,
            &m_F0Metallic)
        );

    const CD3D11_SHADER_RESOURCE_VIEW_DESC srvDesc2(m_F0Metallic.Get(), D3D11_SRV_DIMENSION_TEXTURE1D);
    ThrowIfFailed(
        m_Device->CreateShaderResourceView(
            m_F0Metallic.Get(),
            &srvDesc2,
            &m_F0MetallicSrv)
        );
}