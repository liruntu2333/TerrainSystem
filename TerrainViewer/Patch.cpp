#define NOMINMAX
#include "Patch.h"

#include <string>
#include <directxtk/BufferHelpers.h>
#include "D3DHelper.h"
#include "Vertex.h"
#include <DirectXColors.h>

#include "../HeightMapSplitter/ThreadPool.h"

using namespace DirectX;
using namespace SimpleMath;

Patch::Patch(const std::filesystem::path& path, int x, int y, ID3D11Device* device) : m_X(x), m_Y(y)
{
    m_Resource = LoadResource(path, LOWEST_LOD, device);
}

Patch::RenderResource Patch::GetResource(const std::filesystem::path& path, int lod, ID3D11Device* device)
{
    using namespace std::chrono_literals;
    if (m_Stream.valid() && m_Stream.wait_for(0s) == std::future_status::ready)
    {
        m_Resource = m_Stream.get();
        m_Lod = m_LodStreaming;
        m_LodStreaming = -1;
    }

    if (lod != m_Lod && !m_Stream.valid())
    {
        m_Stream = g_ThreadPool.enqueue([this, &path, lod, device]
        {
            return LoadResource(path, lod, device);
        });
        m_LodStreaming = lod;
    }

    return {
        m_Resource->Vb.Get(),
        m_Resource->Ib.Get(),
        m_Resource->IdxCnt,
        0,
        XMINT2(m_X, m_Y),
        m_Resource->Idx16Bit,
    };
}

std::shared_ptr<Patch::LodResource> Patch::LoadResource(
    const std::filesystem::path& path, int lod, ID3D11Device* device) const
{
    const auto vtx = LoadBinary<MeshVertex>(path.string() + "/" +
        std::to_string(m_X) + "_" + std::to_string(m_Y) + "/lod" + std::to_string(lod) + ".vtx");
    const auto idx = LoadBinary<std::byte>(path.string() + "/" +
        std::to_string(m_X) + "_" + std::to_string(m_Y) + "/lod" + std::to_string(lod) + ".idx");
    auto r = std::make_shared<LodResource>();
    ThrowIfFailed(CreateStaticBuffer(
        device,
        vtx,
        D3D11_BIND_VERTEX_BUFFER,
        &r->Vb
        ));
    ThrowIfFailed(CreateStaticBuffer(
        device,
        idx,
        D3D11_BIND_INDEX_BUFFER,
        &r->Ib
        ));
    if (vtx.size() <= std::numeric_limits<uint16_t>::max())
        r->Idx16Bit = true;
    const size_t indexStride = r->Idx16Bit ? sizeof(uint16_t) : sizeof(uint32_t);
    r->IdxCnt = static_cast<uint32_t>(idx.size() / indexStride);
    std::printf("Patch %2d %2d lod %d loaded\n", m_X, m_Y, lod);
    return r;
}
