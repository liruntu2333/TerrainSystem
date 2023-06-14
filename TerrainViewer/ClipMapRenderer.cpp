#include "ClipMapRenderer.h"
#include <d3dcompiler.h>
#include <DirectXColors.h>
#include <string>
#include <directxtk/BufferHelpers.h>
#include "D3DHelper.h"
#include "MeshVertex.h"

using namespace DirectX;
using namespace SimpleMath;
using Vertex = GridVertex;

ClipMapRenderer::ClipMapRenderer(
    ID3D11Device* device,
    const std::shared_ptr<ConstantBuffer<PassConstants>>& passConst) :
    Renderer(device), m_Cb0(passConst), m_Cb1(m_Device) {}

void ClipMapRenderer::Initialize(ID3D11DeviceContext* context, const std::filesystem::path& path)
{
    auto loadMesh = [this](const std::filesystem::path& p, auto& vb, auto& ib, int& idxCnt)
    {
        const auto meshVertices = LoadBinary<MeshVertex>(p.string() + ".vtx");
        std::vector<Vertex> grid;
        grid.reserve(meshVertices.size());
        std::transform(
            meshVertices.begin(),
            meshVertices.end(),
            std::back_inserter(grid),
            [](const MeshVertex& v)
            {
                return Vertex(Vector2(
                    static_cast<float>(v.PositionX) / 254.0f,
                    static_cast<float>(v.PositionY) / 254.0f
                    ));
            });

        const auto idx = LoadBinary<std::uint16_t>(p.string() + ".idx");
        idxCnt = static_cast<int>(idx.size());
        ThrowIfFailed(CreateStaticBuffer(
            m_Device,
            grid,
            D3D11_BIND_VERTEX_BUFFER,
            vb.GetAddressOf()));
        ThrowIfFailed(CreateStaticBuffer(
            m_Device,
            idx,
            D3D11_BIND_INDEX_BUFFER,
            ib.GetAddressOf()));
    };

    loadMesh(path / "block", m_BlockVb, m_BlockIb, m_BlockIdxCnt);
    loadMesh(path / "center", m_CenterVb, m_CenterIb, m_CenterIdxCnt);
    loadMesh(path / "ring", m_RingVb, m_RingIb, m_RingIdxCnt);
    loadMesh(path / "lt", m_TrimVb[0], m_TrimIb[0], m_TrimIdxCnt);
    loadMesh(path / "rt", m_TrimVb[1], m_TrimIb[1], m_TrimIdxCnt);
    loadMesh(path / "lb", m_TrimVb[2], m_TrimIb[2], m_TrimIdxCnt);
    loadMesh(path / "rb", m_TrimVb[3], m_TrimIb[3], m_TrimIdxCnt);

    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    ThrowIfFailed(D3DReadFileToBlob(L"./shader/GridVS.cso", &blob));

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

    ThrowIfFailed(D3DReadFileToBlob(L"./shader/GridPS.cso", &blob));
    ThrowIfFailed(
        m_Device->CreatePixelShader(
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            nullptr,
            &m_Ps)
        );
}

void ClipMapRenderer::Render(ID3D11DeviceContext* context)
{
    constexpr UINT stride = sizeof(Vertex);
    constexpr UINT offset = 0;
    context->IASetInputLayout(m_InputLayout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11Buffer* cbs[] = { m_Cb0->GetBuffer(), m_Cb1.GetBuffer() };
    context->VSSetConstantBuffers(0, 2, &cbs[0]);
    context->VSSetShader(m_Vs.Get(), nullptr, 0);
    context->OMSetBlendState(s_CommonStates->Opaque(), nullptr, 0xffffffff);
    context->OMSetDepthStencilState(s_CommonStates->DepthDefault(), 0);
    context->RSSetState(s_CommonStates->Wireframe());  // CullCounterClockwise());
    context->PSSetShader(m_Ps.Get(), nullptr, 0);
    context->PSSetConstantBuffers(0, 2, &cbs[0]);

    constexpr int lvMax = 9;
    Vector2 parentOffset(-254.0f * static_cast<float>(1 << (lvMax - 1)));
    int lv = lvMax;
    float scale;
    for (; lv >= 0; --lv)
    {
        scale = 1 << lv;
        // https://developer.nvidia.com/sites/all/modules/custom/gpugems/books/GPUGems2/elementLinks/02_clipmaps_05.jpg
        Vector2 offsets[] =
        {
            Vector2 { 0, 0 },
            Vector2 { M - 1, 0 },
            Vector2 { 2 * M, 0 },
            Vector2 { 3 * M - 1, 0 },

            Vector2 { 0, M - 1 },
            Vector2 { 3 * M - 1, M - 1 },

            Vector2 { 0, 2 * M },
            Vector2 { 3 * M - 1, 2 * M },

            Vector2 { 0, 3 * M - 1 },
            Vector2 { M - 1, 3 * M - 1 },
            Vector2 { 2 * M, 3 * M - 1 },
            Vector2 { 3 * M - 1, 3 * M - 1 },

            Vector2 { 0, 0 },
        };
        for (auto& ofs : offsets)
        {
            ofs *= scale;
            ofs += parentOffset;
        }

        context->IASetVertexBuffers(0, 1, m_BlockVb.GetAddressOf(), &stride, &offset);
        context->IASetIndexBuffer(m_BlockIb.Get(), DXGI_FORMAT_R16_UINT, 0);
        for (int i = 0; i < 12; ++i)
        {
            ObjectConstants obj;
            obj.ScaleFactor = Vector4(scale * 254.0f, scale * 254.0f, offsets[i].x, offsets[i].y);
            obj.Color = i == 0 || i == 2 || i == 5 || i == 11 || i == 6 || i == 9
                            ? Colors::DarkGreen
                            : Colors::Purple;
            m_Cb1.SetData(context, obj);
            context->DrawIndexed(m_BlockIdxCnt, 0, 0);
        }

        context->IASetVertexBuffers(0, 1, m_RingVb.GetAddressOf(), &stride, &offset);
        context->IASetIndexBuffer(m_RingIb.Get(), DXGI_FORMAT_R16_UINT, 0);
        {
            ObjectConstants obj;
            obj.Color = Colors::Gold;
            obj.ScaleFactor = Vector4(scale * 254.0f, scale * 254.0f, offsets[12].x, offsets[12].y);
            m_Cb1.SetData(context, obj);
        }
        context->DrawIndexed(m_RingIdxCnt, 0, 0);

        const int trimId = lv % 4;
        context->IASetVertexBuffers(0, 1, m_TrimVb[trimId].GetAddressOf(), &stride, &offset);
        context->IASetIndexBuffer(m_TrimIb[trimId].Get(), DXGI_FORMAT_R16_UINT, 0);
        {
            ObjectConstants obj;
            obj.Color = Colors::RoyalBlue;
            obj.ScaleFactor = Vector4(scale * 254.0f, scale * 254.0f, offsets[12].x, offsets[12].y);
            m_Cb1.SetData(context, obj);
        }
        context->DrawIndexed(m_TrimIdxCnt, 0, 0);

        constexpr Vector2 newOfs[4] =
        {
            Vector2(M, M),
            Vector2(M - 1, M),
            Vector2(M, M - 1),
            Vector2(M - 1, M - 1),
        };
        parentOffset += newOfs[trimId] * scale;
    }

    context->IASetVertexBuffers(0, 1, m_CenterVb.GetAddressOf(), &stride, &offset);
    context->IASetIndexBuffer(m_CenterIb.Get(), DXGI_FORMAT_R16_UINT, 0);
    ObjectConstants obj;
    obj.Color = Colors::DarkRed;
    obj.ScaleFactor = Vector4(scale * 254.0f, scale * 254.0f, parentOffset.x, parentOffset.y);
    m_Cb1.SetData(context, obj);
    context->DrawIndexed(m_CenterIdxCnt, 0, 0);
}
