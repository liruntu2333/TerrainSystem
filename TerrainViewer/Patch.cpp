#define NOMINMAX
#include "Patch.h"

#include <set>
#include <string>
#include <directxtk/BufferHelpers.h>
#include "D3DHelper.h"
#include "TerrainVertex.h"
#include <DirectXColors.h>

using namespace DirectX;
using namespace SimpleMath;

static const PackedVector::XMCOLOR G_COLORS[] =
{
    { 1.000000000f, 0.000000000f, 0.000000000f, 1.000000000f },
    { 0.000000000f, 0.501960814f, 0.000000000f, 1.000000000f },
    { 0.000000000f, 0.000000000f, 1.000000000f, 1.000000000f },
    { 1.000000000f, 1.000000000f, 0.000000000f, 1.000000000f },
    { 0.000000000f, 1.000000000f, 1.000000000f, 1.000000000f },
};

static const std::set G_DISTANCES =
{
    300.0f,
    600.0f,
    1000.0f,
    1500.0f,
    2100.0f,
};

Patch::Patch(const std::filesystem::path& path, ID3D11Device* device)
{
    const std::string name = path.u8string();
    const auto sx = name.substr(name.find_last_of('/') + 1);
    const auto sy = sx.substr(sx.find_last_of('_') + 1);
    m_X = std::stoi(sx, nullptr);
    m_Y = std::stoi(sy, nullptr);

    for (int i = 0; i < 5; ++i)
    {
        const auto vtx = LoadBinary<TerrainVertex>(path.string() + "/lod" + std::to_string(i) + ".vtx");
        ThrowIfFailed(CreateStaticBuffer(
            device,
            vtx,
            D3D11_BIND_VERTEX_BUFFER,
            &m_Lods[i].Vb
            ));

        const auto idx = LoadBinary<std::byte>(path.string() + "/lod" + std::to_string(i) + ".idx");
        ThrowIfFailed(CreateStaticBuffer(
            device,
            idx,
            D3D11_BIND_INDEX_BUFFER,
            &m_Lods[i].Ib
            ));

        if (vtx.size() <= std::numeric_limits<uint16_t>::max())
            m_Lods[i].Idx16Bit = true;
        const size_t indexStride = m_Lods[i].Idx16Bit ? sizeof(uint16_t) : sizeof(uint32_t);
        m_Lods[i].IdxCnt = static_cast<uint32_t>(idx.size() / indexStride);
    }
}

Patch::RenderResource Patch::GetResource(const Vector3& camera, const XMINT2& cameraOffset) const
{
    //return m_RenderResources[0];
    const auto offset = XMINT2(m_X - cameraOffset.x, -m_Y - cameraOffset.y);
    const auto center = Vector3((offset.x + 0.5f) * PATCH_SIZE, 1200, (offset.y - 0.5f) * PATCH_SIZE);
    const auto dist = (camera - center).Length();
    const auto upper = G_DISTANCES.upper_bound(dist);
    const int lod = upper == G_DISTANCES.end() ? 4 : std::distance(G_DISTANCES.begin(), upper);

    return {
        m_Lods[lod].Vb.Get(),
        m_Lods[lod].Ib.Get(),
        m_Lods[lod].IdxCnt,
        G_COLORS[lod].c,
        offset,
        XMUINT2(m_X * PATCH_SIZE, m_Y * PATCH_SIZE),
        m_Lods[lod].Idx16Bit,
    };
}
