#define NOMINMAX
#include "PlanetRenderer.h"

#include <array>
#include <d3dcompiler.h>
#include <DirectXColors.h>
#include <map>
#include <directxtk/BufferHelpers.h>

#include "D3DHelper.h"

using namespace DirectX;
using namespace SimpleMath;

namespace
{
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
            Quaternion::CreateFromYawPitchRoll(XM_PI, 0, 0)
        };
        return ORIENTATIONS[face];
    }

    Plane GetCubePlane(const CubeFace face)
    {
        switch (face)
        {
        case POSITIVE_X:
            return Plane(Vector3::Right, -1.0f);
        case NEGATIVE_X:
            return Plane(Vector3::Left, -1.0f);
        case POSITIVE_Y:
            return Plane(Vector3::Up, -1.0f);
        case NEGATIVE_Y:
            return Plane(Vector3::Down, -1.0f);
        case POSITIVE_Z:
            return Plane(Vector3::Backward, -1.0f);
        case NEGATIVE_Z:
            return Plane(Vector3::Forward, -1.0f);
        }
        return Plane(Vector3::Zero, 0.0f);
    }

    Vector3 GetCubeVertex(const CubeFace face, const double x, const double y)
    {
        return GetFaceForward(face) +
            GetFaceRight(face) * static_cast<float>(x) +
            GetFaceUp(face) * static_cast<float>(y);
    }

    void MapToCube(const Vector3& ov, CubeFace& face, Vector2& xy)
    {
        const auto& [x, y, z]    = ov;
        const auto& [ax, ay, az] = Vector3(std::abs(x), std::abs(y), std::abs(z));

        if (ax >= ay && ax >= az)
        {
            face = x >= 0.0 ? POSITIVE_X : NEGATIVE_X;
        }
        else if (ay >= az && ay >= ax)
        {
            face = y >= 0.0 ? POSITIVE_Y : NEGATIVE_Y;
        }
        else
        {
            face = z >= 0.0 ? POSITIVE_Z : NEGATIVE_Z;
        }

        const Ray r(Vector3::Zero, ov);
        float dist = 0.0f;
        assert(r.Intersects(GetCubePlane(face), dist));
        Vector3 p = r.position + r.direction * dist;
        p -= GetFaceForward(face);
        xy = Vector2(p.Dot(GetFaceRight(face)), p.Dot(GetFaceUp(face)));
    }

    constexpr int maxTreeDepth       = 10;
    constexpr int cubeResolution     = 255;
    constexpr double maxCubeLength   = 2.0;
    constexpr double maxCubeGridSize = maxCubeLength / (cubeResolution - 1);
    constexpr double ratio           = 0.4;

    constexpr double interiorCubeLength = 0.57735026918962576450914878050196; // std::sqrt(1.0 / 3.0)

    struct SphericalCubeQuadTreeNode
    {
        int depth;
        double gridSize;
        double length;
        double halfLength;
        Vector2 offset;
        CubeFace face;
        std::array<std::unique_ptr<SphericalCubeQuadTreeNode>, 4> children;

        SphericalCubeQuadTreeNode()                                            = default;
        SphericalCubeQuadTreeNode(const SphericalCubeQuadTreeNode&)            = default;
        SphericalCubeQuadTreeNode(SphericalCubeQuadTreeNode&&)                 = default;
        SphericalCubeQuadTreeNode& operator=(const SphericalCubeQuadTreeNode&) = default;
        SphericalCubeQuadTreeNode& operator=(SphericalCubeQuadTreeNode&&)      = default;

        SphericalCubeQuadTreeNode(const CubeFace face, const int depth, const Vector2& offset) :
            depth(depth), gridSize(maxCubeGridSize / (1 << depth)),
            length(maxCubeLength / (1 << depth)), halfLength(length * 0.5), offset(offset), face(face) {}

        [[nodiscard]] std::array<Vector3, 4> GetCubicCorners() const
        {
            return
            {
                GetCubeVertex(face, offset.x, offset.y),
                GetCubeVertex(face, offset.x + length, offset.y),
                GetCubeVertex(face, offset.x, offset.y + length),
                GetCubeVertex(face, offset.x + length, offset.y + length)
            };
        }

        [[nodiscard]] std::array<Vector3, 4> GetSphericalCorners(const Matrix& wld) const
        {
            std::array<Vector3, 4> corners = GetCubicCorners();
            std::transform(corners.begin(), corners.end(), corners.begin(),
                [&wld](auto&& v)
                {
                    v.Normalize();
                    v = Vector3::Transform(v, wld);
                    return v;
                });
            return corners;
        }

        [[nodiscard]] std::array<Vector3, 4> GetUnitSphericalCorners(const Quaternion& rot) const
        {
            std::array<Vector3, 4> corners = GetCubicCorners();
            std::transform(corners.begin(), corners.end(), corners.begin(),
                [&rot](auto&& v)
                {
                    v.Normalize();
                    v = Vector3::Transform(v, rot);
                    return v;
                });
            return corners;
        }

        [[nodiscard]] BoundingFrustum GetBounding(const Matrix& wld, const float elevationRatio) const
        {
            float n = 1.0, f = 0.0f;
            if (depth == 0) // 1 / 6 of the sphere, near far lie on a cosine curve
            {
                n = interiorCubeLength - elevationRatio;
                f = 1.0f + elevationRatio;
            }
            else // otherwise near far lie on a monotonic curve
            {
                const auto& fwd = GetFaceForward(face);
                for (auto&& corner : GetUnitSphericalCorners(Quaternion::Identity))
                {
                    const float vDotFwd = corner.Dot(fwd);

                    n = std::min(n, vDotFwd * (1.0f - elevationRatio));
                    f = std::max(f, vDotFwd * (1.0f + elevationRatio));
                }
            }

            auto frustum = BoundingFrustum(
                Vector3::Zero,
                GetFaceOrientation(face),
                offset.x,
                offset.x + length,
                offset.y + length,
                offset.y,
                n,
                f
                );

            frustum.Transform(frustum, wld);
            return frustum;
        }

        [[nodiscard]] Vector3 GetCenter(const Matrix& wld) const
        {
            auto v = GetCubeVertex(face, offset.x + halfLength, offset.y + halfLength);
            v.Normalize();
            return Vector3::Transform(v, wld);
        }

        [[nodiscard]] float GetSphericalTileLength(const float scl) const
        {
            return length * XM_PIDIV2 * scl;
        }

        static std::unique_ptr<SphericalCubeQuadTreeNode> CreateTree(
            CubeFace face, const Vector2& ofs, const int maxDepth, int depth = 0)
        {
            if (depth > maxDepth) return nullptr;

            auto root         = std::make_unique<SphericalCubeQuadTreeNode>(face, depth, ofs);
            root->children[0] = CreateTree(face, ofs + Vector2::Zero, maxDepth, depth + 1);
            root->children[1] = CreateTree(face, ofs + Vector2(root->halfLength, 0), maxDepth, depth + 1);
            root->children[2] = CreateTree(face, ofs + Vector2(0, root->halfLength), maxDepth, depth + 1);
            root->children[3] = CreateTree(face, ofs + Vector2(root->halfLength, root->halfLength), maxDepth, depth + 1);
            return root;
        }

        static void TraverseTree(
            SphericalCubeQuadTreeNode* node,
            const BoundingFrustum& frustum,
            const float planetScl,
            const float elevationRatio,
            const Quaternion& planetRot,
            const Vector3& planetTrans,
            const Matrix& planetWld,
            const float horizonSqr,
            std::map<float, SphericalCubeQuadTreeNode*>& visibleNodes,
            bool contain)
        {
            if (!node)
            {
                return;
            }
            const float cosAng   = planetScl / Vector3(frustum.Origin - planetTrans).Length();
            const Vector3 camPos = frustum.Origin;
            Vector3 camDir;
            camPos.Normalize(camDir);

            // if (camXy == nullptr ||
            //     !(camXy->x > node->offset.x && camXy->x < node->offset.x + node->length &&
            //         camXy->y > node->offset.y && camXy->y < node->offset.y + node->length))
            // {
            //     bool back = true;
            //     for (auto&& corner : node->GetUnitSphericalCorners(planetRot))
            //     {
            //         if (corner.Dot(camDir) >= cosAng)
            //         {
            //             back = false;
            //             break;
            //         }
            //     }
            //     if (back) return;
            // }

            // auto corners = node->GetSphericalCorners(planetWld);
            // bd.GetPlanes(&planes[0], &planes[1], &planes[2], &planes[3], &planes[4], &planes[5]);
            // if (std::all_of(corners.begin(), corners.end(),
            //     [horizonSqr, &camPos](const auto& corner)
            //     {
            //         return (camPos - corner).LengthSquared() > horizonSqr;
            //     }))
            // {
            //     // __debugbreak();
            //     return;
            // }
            auto center = node->GetCenter(planetWld);
            // if ((camPos - center).LengthSquared() > horizonSqr)
            // {
            //     return;
            // }
            auto bd = node->GetBounding(planetWld, elevationRatio);
            if (!contain)
            {
                const ContainmentType containment = frustum.Contains(bd);
                if (containment == DISJOINT)
                {
                    return;
                }
                if (containment == CONTAINS)
                {
                    contain = true;
                }
            }

            const float dist = (center - Vector3(frustum.Origin)).Length();
            float tileLen    = node->GetSphericalTileLength(planetScl);
            if (tileLen * ratio > dist && node->depth < maxTreeDepth)
            {
                // TraverseTree(node->children[1].get(), frustum, planetScl, elevationRatio, planetRot, planetTrans, planetWld, camXy, visibleNodes, bs);
                for (auto&& child : node->children)
                {
                    TraverseTree(child.get(), frustum, planetScl, elevationRatio, planetRot, planetTrans, planetWld,
                        horizonSqr, visibleNodes, contain);
                }
            }
            else
            {
                visibleNodes.emplace(dist, node);
            }
        }
    };

    std::array<std::unique_ptr<SphericalCubeQuadTreeNode>, 6> roots = { nullptr };
}

void PlanetRenderer::Initialize(const std::filesystem::path& shaderDir)
{
    for (int i = 0; i < 6; ++i)
    {
        if (!roots[i])
            roots[i] = SphericalCubeQuadTreeNode::CreateTree(static_cast<CubeFace>(i), Vector2(-1, -1), maxTreeDepth);
    }
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

    name = shaderDir / "BoundVS.cso";
    ThrowIfFailed(D3DReadFileToBlob(name.c_str(), &blob));
    ThrowIfFailed(
        m_Device->CreateVertexShader(
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            nullptr,
            &m_BoundVs)
        );

    ThrowIfFailed(
        m_Device->CreateInputLayout(
            VertexPosition::InputElements,
            VertexPosition::InputElementCount,
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            &m_VertPosLayout)
        );

    name = shaderDir / "BoundPS.cso";
    ThrowIfFailed(D3DReadFileToBlob(name.c_str(), &blob));
    ThrowIfFailed(
        m_Device->CreatePixelShader(
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            nullptr,
            &m_BoundPs)
        );

    const CD3D11_BUFFER_DESC bd(sizeof(VertexPosition) * 8, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC,
        D3D11_CPU_ACCESS_WRITE);
    ThrowIfFailed(m_Device->CreateBuffer(&bd, nullptr, &m_BoundVb));
    // Returns 8 corners position of bounding frustum.
    //     Near    Far
    //    0----1  4----5
    //    |    |  |    |
    //    |    |  |    |
    //    3----2  7----6
    const std::vector<uint16_t> boundIdx {
        0, 2, 1, 0, 3, 2, // Near
        4, 5, 7, 7, 5, 6, // Far
        0, 1, 5, 0, 5, 4, // Top
        2, 7, 6, 2, 3, 7, // Bottom
        0, 4, 7, 0, 7, 3, // Left
        5, 2, 6, 5, 1, 2 // Right
    };
    ThrowIfFailed(CreateStaticBuffer(m_Device, boundIdx, D3D11_BIND_INDEX_BUFFER, &m_BoundIb));

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
    m_Cb1.Create(m_Device);
}

void PlanetRenderer::Render(
    ID3D11DeviceContext* context,
    Uniforms uniforms,
    const BoundingFrustum& frustum,
    const Quaternion& rot,
    const Vector3& trans,
    const bool wireFrame, const bool freeze, const bool debug)
{
    std::map<float, SphericalCubeQuadTreeNode*> nodes;

    const Vector3 camPos = frustum.Origin;
    CubeFace camFace;
    Vector2 camXy;
    Quaternion invRot;
    rot.Inverse(invRot);
    Vector3 ov = camPos - trans;
    ov.Normalize();
    ov = Vector3::Transform(ov, invRot);
    MapToCube(ov, camFace, camXy);

    const Matrix wld =
        Matrix::CreateScale(uniforms.radius) *
        Matrix::CreateFromQuaternion(rot) *
        Matrix::CreateTranslation(trans);

    const double elevationRatio = uniforms.elevation / uniforms.radius * uniforms.baseAmplitude * 1.2f;
    float horizon2              = (camPos - trans).LengthSquared() - uniforms.radius * uniforms.radius;
    horizon2 *= 1.5;
    for (int i = 0; i < 6; ++i)
    {
        SphericalCubeQuadTreeNode::TraverseTree(
            roots[i].get(),
            frustum, uniforms.radius, elevationRatio,
            rot, trans, wld, horizon2, nodes, false);
    }

    ID3D11Buffer* cbs[]       = { m_Cb0.GetBuffer(), m_Cb1.GetBuffer() };
    constexpr uint32_t stride = sizeof(VertexPosition), offset = 0;

    {
        int ii = 0;
        for (auto&& [dist, node] : nodes)
        {
            if (ii >= maxInstance) break;

            uniforms.instances[ii++] = Uniforms::Instance
            {
                GetFaceForward(node->face),
                static_cast<float>(node->gridSize),
                GetFaceRight(node->face),
                node->offset.x,
                GetFaceUp(node->face),
                node->offset.y
            };
        }

        m_Cb0.SetData(context, uniforms);

        context->VSSetConstantBuffers(0, 1, cbs);
        context->PSSetConstantBuffers(0, 1, cbs);

        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->IASetInputLayout(nullptr);

        ID3D11Buffer* vb = nullptr;
        context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        context->IASetIndexBuffer(m_TileIb.Get(), DXGI_FORMAT_R16_UINT, 0);
        context->VSSetShader(m_PlanetVs.Get(), nullptr, 0);

        context->RSSetState(wireFrame ? s_CommonStates->Wireframe() : s_CommonStates->CullClockwise());

        context->PSSetShader(m_PlanetPs.Get(), nullptr, 0);

        ID3D11ShaderResourceView* srv[] = { m_AlbedoRoughnessSrv.Get(), m_F0MetallicSrv.Get() };
        context->PSSetShaderResources(0, _countof(srv), srv);
        ID3D11SamplerState* sam[] = { s_CommonStates->LinearClamp() };
        context->PSSetSamplers(0, _countof(sam), sam);

        context->OMSetDepthStencilState(s_CommonStates->DepthReverseZ(), 0);
        context->OMSetBlendState(s_CommonStates->Opaque(), nullptr, 0xFFFFFFFF);

        context->DrawIndexedInstanced(m_IndicesPerFace, ii, 0, 0, 0);
    }

    /// Rendering Ocean
    if (!wireFrame && uniforms.oceanLevel > -1.5f)
    {
        uniforms.radius += uniforms.elevation * uniforms.oceanLevel - 1.0f;
        context->VSSetShader(m_OceanVs.Get(), nullptr, 0);
        context->PSSetShader(m_OceanPs.Get(), nullptr, 0);
        context->OMSetDepthStencilState(s_CommonStates->DepthReadReverseZ(), 0);
        context->OMSetBlendState(s_CommonStates->AlphaBlend(), nullptr, 0xFFFFFFFF);

        for (int i = 0; i < 6; ++i)
        {
            auto& [fwd, sz, rht, x, up, y] = uniforms.instances[i];

            const auto node = roots[i].get();

            fwd = GetFaceForward(node->face);
            up  = GetFaceUp(node->face);
            rht = GetFaceRight(node->face);
            sz  = node->gridSize;
            x   = node->offset.x;
            y   = node->offset.y;
        }
        m_Cb0.SetData(context, uniforms);
        context->DrawIndexedInstanced(m_IndicesPerFace, 6, 0, 0, 0);
    }

    if (!debug)
    {
        return;
    }
    /// Render boundings
    {
        BoundingUniforms u1;
        int ii = 0;
        for (auto&& [dist, node] : nodes)
        {
            if (ii >= maxBound) break;
            Vector3 cs[8];
            node->GetBounding(wld, elevationRatio).GetCorners(cs);
            for (int j = 0; j < 8; ++j)
            {
                auto& [vx, vy, vz] = cs[j];
                u1.corners[ii][j]  = Vector4(vx, vy, vz, 1.0f);
            }
            ++ii;
        }
        if (freeze)
        {
            Vector3 cs[8];
            frustum.GetCorners(cs);
            for (int j = 0; j < 8; ++j)
            {
                auto& [vx, vy, vz] = cs[j];
                u1.corners[ii][j]  = Vector4(vx, vy, vz, 1.0f);
            }
        }

        m_Cb0.SetData(context, uniforms);
        m_Cb1.SetData(context, u1);

        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->IASetInputLayout(m_VertPosLayout.Get());
        context->IASetIndexBuffer(m_BoundIb.Get(), DXGI_FORMAT_R16_UINT, 0);
        context->VSSetShader(m_BoundVs.Get(), nullptr, 0);
        context->VSSetConstantBuffers(0, _countof(cbs), cbs);
        context->PSSetShader(m_BoundPs.Get(), nullptr, 0);
        context->RSSetState(s_CommonStates->Wireframe());
        context->OMSetDepthStencilState(s_CommonStates->DepthReverseZ(), 0);
        ID3D11Buffer* vb = nullptr;
        context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        context->DrawIndexedInstanced(36, std::min(maxBound, static_cast<int>(nodes.size())) + static_cast<int>(freeze), 0, 0, 0);
    }
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

    ThrowIfFailed(CreateStaticBuffer(m_Device, indices, D3D11_BIND_INDEX_BUFFER, &m_TileIb));
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
