#include "PlanetRenderer.h"

#include <array>
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


    const Vector3& GetFaceForward(const CubeFace face)
    {
        switch (face)
        {
        case POSITIVE_X:
            return Vector3::Right;
        case NEGATIVE_X:
            return Vector3::Left;
        case POSITIVE_Y:
            return Vector3::Up;
        case NEGATIVE_Y:
            return Vector3::Down;
        case POSITIVE_Z:
            return Vector3::Backward;
        case NEGATIVE_Z:
            return Vector3::Forward;
        }
        return Vector3::Zero;
    }

    const Vector3& GetFaceRight(const CubeFace face)
    {
        switch (face)
        {
        case POSITIVE_X:
            return Vector3::Forward;
        case NEGATIVE_X:
            return Vector3::Backward;
        case POSITIVE_Y:
            return Vector3::Right;
        case NEGATIVE_Y:
            return Vector3::Right;
        case POSITIVE_Z:
            return Vector3::Right;
        case NEGATIVE_Z:
            return Vector3::Left;
        }
        return Vector3::Zero;
    }

    const Vector3& GetFaceUp(const CubeFace face)
    {
        switch (face)
        {
        case POSITIVE_X:
            return Vector3::Up;
        case NEGATIVE_X:
            return Vector3::Up;
        case POSITIVE_Y:
            return Vector3::Forward;
        case NEGATIVE_Y:
            return Vector3::Backward;
        case POSITIVE_Z:
            return Vector3::Up;
        case NEGATIVE_Z:
            return Vector3::Up;
        }
        return Vector3::Zero;
    }

    const Quaternion& GetFaceOrientation(const CubeFace face)
    {
        static const Quaternion ORIENTATIONS[] =
        {
            Quaternion::CreateFromYawPitchRoll(XM_PIDIV2, 0, 0),
            Quaternion::CreateFromYawPitchRoll(-XM_PIDIV2, 0, 0),
            Quaternion::CreateFromYawPitchRoll(0, -XM_PIDIV2, 0),
            Quaternion::CreateFromYawPitchRoll(0, XM_PIDIV2, 0),
            Quaternion::CreateFromYawPitchRoll(0, 0, 0),
            Quaternion::CreateFromYawPitchRoll(0, XM_PI, 0)
        };
        return ORIENTATIONS[face];
    }

    Vector3 GetCubeVertex(CubeFace face, const double x, const double y)
    {
        return GetFaceForward(face) +
            GetFaceRight(face) * static_cast<float>(x) +
            GetFaceUp(face) * static_cast<float>(y);
    }

    constexpr int cubeResolution     = 255;
    constexpr double maxCubeLength   = 2.0;
    constexpr double maxCubeGridSize = maxCubeLength / (cubeResolution - 1);
    constexpr float angleThreshold   = -0.05f;

    constexpr double interiorCubeLength = 0.70710678118654752440084436210485; // sqrt(0.5)

    struct SphericalCubeQuadTreeNode
    {
        int depth;
        double gridSize;
        double length;
        double halfLength;
        Vector2 offset;
        CubeFace face;

        SphericalCubeQuadTreeNode()                                            = default;
        SphericalCubeQuadTreeNode(const SphericalCubeQuadTreeNode&)            = default;
        SphericalCubeQuadTreeNode(SphericalCubeQuadTreeNode&&)                 = default;
        SphericalCubeQuadTreeNode& operator=(const SphericalCubeQuadTreeNode&) = default;
        SphericalCubeQuadTreeNode& operator=(SphericalCubeQuadTreeNode&&)      = default;

        SphericalCubeQuadTreeNode(const CubeFace face, const int depth, const Vector2& offset) :
            depth(depth), gridSize(maxCubeGridSize / (1 << depth)),
            length(maxCubeLength / (1 << depth)), halfLength(length * 0.5), offset(offset), face(face) {}

        std::array<SphericalCubeQuadTreeNode, 4> GetChildren() const
        {
            return
            {
                SphericalCubeQuadTreeNode(face, depth + 1, offset),
                SphericalCubeQuadTreeNode(face, depth + 1, offset + Vector2(halfLength, 0)),
                SphericalCubeQuadTreeNode(face, depth + 1, offset + Vector2(0, halfLength)),
                SphericalCubeQuadTreeNode(face, depth + 1, offset + Vector2(halfLength, halfLength))
            };
        }

        [[nodiscard]] BoundingFrustum GetBounding(const Matrix& wld) const
        {
            auto frustum = BoundingFrustum(
                Vector3::Zero,
                GetFaceOrientation(face),
                offset.x + length,
                offset.x,
                offset.y + length,
                offset.y,
                interiorCubeLength,
                1.0
                );
            frustum.Transform(frustum, wld);
            return frustum;
        }

        [[nodiscard]] std::array<Vector3, 4> GetCornerNormals(const Matrix& rot) const
        {
            std::array<Vector3, 4> corners =
            {
                GetCubeVertex(face, offset.x, offset.y),
                GetCubeVertex(face, offset.x + length, offset.y),
                GetCubeVertex(face, offset.x, offset.y + length),
                GetCubeVertex(face, offset.x + length, offset.y + length)
            };
            for (auto&& corner : corners)
            {
                corner = Vector3::TransformNormal(corner, rot);
                corner.Normalize();
            }

            return std::move(corners);
        }

        static void Traverse(const SphericalCubeQuadTreeNode& node,
                             const BoundingFrustum& frustum, const Matrix& wld,
                             std::vector<SphericalCubeQuadTreeNode>& nodes)
        {
            auto camFront = Vector3::TransformNormal(Vector3::Forward,
                Matrix::CreateFromQuaternion(frustum.Orientation));
            camFront.Normalize();

            if (auto ns = node.GetCornerNormals(wld); std::all_of(ns.begin(), ns.end(),
                [&camFront](auto&& n) { return camFront.Dot(n) > angleThreshold; }))
            {
                return;
            }

            if (frustum.Contains(node.GetBounding(wld)) == DISJOINT)
            {
                return;
            }

            nodes.push_back(node);
        }
    };
}

void PlanetRenderer::Initialize(const std::filesystem::path& shaderDir)
{
    CreateSphere(cubeResolution);
    CreateTexture();

    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    auto name = (shaderDir / "PlanetVS.cso").wstring();
    ThrowIfFailed(D3DReadFileToBlob(name.c_str(), &blob));

    ThrowIfFailed(
        m_Device->CreateVertexShader(
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            nullptr,
            &m_PlanetVs)
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
            &m_PlanetPs)
        );

    name = shaderDir / "OceanVS.cso";
    ThrowIfFailed(D3DReadFileToBlob(name.c_str(), &blob));
    ThrowIfFailed(
        m_Device->CreateVertexShader(
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            nullptr,
            &m_OceanVs)
        );

    name = shaderDir / "OceanPS.cso";
    ThrowIfFailed(D3DReadFileToBlob(name.c_str(), &blob));
    ThrowIfFailed(
        m_Device->CreatePixelShader(
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            nullptr,
            &m_OceanPs)
        );

    name = shaderDir / "WorldMapCS.cso";
    ThrowIfFailed(D3DReadFileToBlob(name.c_str(), &blob));
    ThrowIfFailed(
        m_Device->CreateComputeShader(
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            nullptr,
            &m_WorldMapCs)
        );

    m_Cb0.Create(m_Device);
}

void PlanetRenderer::Render(ID3D11DeviceContext* context, Uniforms uniforms, const BoundingFrustum& frustum, const Matrix& worldWithScl, const bool wireFrame)
{
    const auto cb = m_Cb0.GetBuffer();
    context->VSSetConstantBuffers(0, 1, &cb);
    context->PSSetConstantBuffers(0, 1, &cb);

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetInputLayout(m_VertexLayout.Get());

    constexpr uint32_t stride = sizeof(Vertex), offset = 0;
    ID3D11Buffer* vb          = nullptr;
    context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    context->IASetIndexBuffer(m_IndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    context->VSSetShader(m_PlanetVs.Get(), nullptr, 0);

    context->RSSetState(wireFrame ? s_CommonStates->Wireframe() : s_CommonStates->CullClockwise());

    context->PSSetShader(m_PlanetPs.Get(), nullptr, 0);

    ID3D11ShaderResourceView* srv[] = { m_AlbedoRoughnessSrv.Get(), m_F0MetallicSrv.Get() };
    context->PSSetShaderResources(0, _countof(srv), srv);
    ID3D11SamplerState* sam[] = { s_CommonStates->LinearClamp() };
    context->PSSetSamplers(0, _countof(sam), sam);

    context->OMSetDepthStencilState(s_CommonStates->DepthReverseZ(), 0);
    context->OMSetBlendState(s_CommonStates->Opaque(), nullptr, 0xFFFFFFFF);

    std::vector<SphericalCubeQuadTreeNode> nodes;
    for (int i = 0; i < 6; ++i)
    {
        SphericalCubeQuadTreeNode::Traverse(
            SphericalCubeQuadTreeNode(static_cast<CubeFace>(i), 0, Vector2(-1, -1)),
            frustum, worldWithScl, nodes);
    }

    // std::sort(nodes.begin(), nodes.end(), [camPos = uniforms.camPos](auto&& a, auto&& b)
    // {
    //     return Vector3::DistanceSquared(a.GetCubeVertex(a.face, a.offset.x, a.offset.y), camPos) <
    //         Vector3::DistanceSquared(b.GetCubeVertex(b.face, b.offset.x, b.offset.y), camPos);
    // });

    for (int i = 0; i < nodes.size(); ++i)
    {
        auto& [fwd, sz, rht, x, up, y] = uniforms.instances[i];

        const auto& node = nodes[i];

        fwd = GetFaceForward(node.face);
        up  = GetFaceUp(node.face);
        rht = GetFaceRight(node.face);
        sz  = node.gridSize;
        x   = node.offset.x;
        y   = node.offset.y;
    }

    m_Cb0.SetData(context, uniforms);
    context->DrawIndexedInstanced(m_IndicesPerFace, nodes.size(), 0, 0, 0);

    if (wireFrame || uniforms.oceanLevel <= -0.1f) return;

    // ocean
    uniforms.radius += uniforms.elevation * uniforms.oceanLevel - 1.0f;
    context->VSSetShader(m_OceanVs.Get(), nullptr, 0);
    context->PSSetShader(m_OceanPs.Get(), nullptr, 0);
    context->OMSetDepthStencilState(s_CommonStates->DepthReadReverseZ(), 0);
    context->OMSetBlendState(s_CommonStates->AlphaBlend(), nullptr, 0xFFFFFFFF);

    for (int i = 0; i < 6; ++i)
    {
        auto& [fwd, sz, rht, x, up, y] = uniforms.instances[i];

        const auto node = SphericalCubeQuadTreeNode(static_cast<CubeFace>(i), 0, Vector2(-1, -1));

        fwd = GetFaceForward(node.face);
        up  = GetFaceUp(node.face);
        rht = GetFaceRight(node.face);
        sz  = node.gridSize;
        x   = node.offset.x;
        y   = node.offset.y;
    }

    m_Cb0.SetData(context, uniforms);
    context->DrawIndexedInstanced(m_IndicesPerFace, 6, 0, 0, 0);
}

void PlanetRenderer::CreateWorldMap(ID3D11DeviceContext* context, const Uniforms& uniforms)
{
    m_Cb0.SetData(context, uniforms);
    ID3D11Buffer* cb0 = m_Cb0.GetBuffer();

    context->CSSetShader(m_WorldMapCs.Get(), nullptr, 0);
    context->CSSetConstantBuffers(0, 1, &cb0);
    ID3D11UnorderedAccessView* uav = m_WorldMap->GetUav();
    context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
    ID3D11ShaderResourceView* srv = m_AlbedoRoughnessSrv.Get();
    context->CSSetShaderResources(0, 1, &srv);
    ID3D11SamplerState* sam = s_CommonStates->LinearClamp();
    context->CSSetSamplers(0, 1, &sam);
    context->Dispatch(kWorldMapWidth / 16, kWorldMapHeight / 16, 1);

    uav = nullptr;
    context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
}

void PlanetRenderer::CreateSphere(const uint16_t tesselation)
{
    std::vector<uint16_t> indices;
    const uint16_t grids = tesselation - 1;

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
            indices.emplace_back(i2);
            indices.emplace_back(i1);

            indices.emplace_back(i2);
            indices.emplace_back(i3);
            indices.emplace_back(i1);
        }
    }

    ThrowIfFailed(CreateStaticBuffer(m_Device, indices, D3D11_BIND_INDEX_BUFFER, &m_IndexBuffer));
    m_Tesselation    = tesselation;
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

    CD3D11_TEXTURE2D_DESC texDesc(DXGI_FORMAT_R8G8B8A8_UNORM, kWorldMapWidth, kWorldMapHeight, 1, 1,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
    m_WorldMap = std::make_unique<Texture2D>(m_Device, texDesc);
    m_WorldMap->CreateViews(m_Device);
}
