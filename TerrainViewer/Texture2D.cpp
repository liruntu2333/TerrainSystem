#include "Texture2D.h"

#include <cassert>

#include "D3DHelper.h"
#include <directxtk/DDSTextureLoader.h>
#include <directxtk/WICTextureLoader.h>

using namespace DirectX;

Texture2D::Texture2D(ID3D11Device* device, const D3D11_TEXTURE2D_DESC& desc) : m_Desc(desc)
{
    const auto hr = device->CreateTexture2D(&m_Desc, nullptr, &m_Texture);
    ThrowIfFailed(hr);
}

Texture2D::Texture2D(ID3D11Device* device, const std::filesystem::path& path)
{
    ThrowIfFailed(CreateTextureFromFile(device, path, m_Texture.GetAddressOf(), m_Srv.GetAddressOf()));
    m_Texture->GetDesc(&m_Desc);
}

Texture2D::Texture2D(
    ID3D11Device* device, const D3D11_TEXTURE2D_DESC& desc, const void* initData, uint32_t systemPitch) :
    m_Desc(desc)
{
    const D3D11_SUBRESOURCE_DATA data { initData, systemPitch, 0 };
    ThrowIfFailed(device->CreateTexture2D(&m_Desc, &data, &m_Texture));
}

void Texture2D::CreateViews(ID3D11Device* device)
{
    D3D11_TEXTURE2D_DESC desc;
    m_Texture->GetDesc(&desc);
    auto bindFlag = desc.BindFlags;
    bool isMultiSample = desc.SampleDesc.Count > 1;

    if (m_Srv == nullptr && bindFlag & D3D11_BIND_SHADER_RESOURCE)
    {
        auto srv = CD3D11_SHADER_RESOURCE_VIEW_DESC(m_Texture.Get(),
            isMultiSample
                ? D3D11_SRV_DIMENSION_TEXTURE2DMS
                : D3D11_SRV_DIMENSION_TEXTURE2D);
        auto hr = device->CreateShaderResourceView(m_Texture.Get(), &srv, &m_Srv);
        ThrowIfFailed(hr);
    }

    if (m_Rtv == nullptr && bindFlag & D3D11_BIND_RENDER_TARGET)
    {
        auto rtv = CD3D11_RENDER_TARGET_VIEW_DESC(m_Texture.Get(),
            isMultiSample
                ? D3D11_RTV_DIMENSION_TEXTURE2DMS
                : D3D11_RTV_DIMENSION_TEXTURE2D);
        auto hr = device->CreateRenderTargetView(m_Texture.Get(), &rtv, &m_Rtv);
        ThrowIfFailed(hr);
    }

    if (m_Uav == nullptr && bindFlag & D3D11_BIND_UNORDERED_ACCESS)
    {
        assert(!isMultiSample); // MSAA resource shouldn't be multi-sampled.
        auto uav = CD3D11_UNORDERED_ACCESS_VIEW_DESC(m_Texture.Get(),
            D3D11_UAV_DIMENSION_TEXTURE2D);
        auto hr = device->CreateUnorderedAccessView(m_Texture.Get(), &uav, &m_Uav);
        ThrowIfFailed(hr);
    }

    if (m_Dsv == nullptr && bindFlag & D3D11_BIND_DEPTH_STENCIL)
    {
        auto dsv = CD3D11_DEPTH_STENCIL_VIEW_DESC(m_Texture.Get(),
            isMultiSample
                ? D3D11_DSV_DIMENSION_TEXTURE2DMS
                : D3D11_DSV_DIMENSION_TEXTURE2D);
        auto hr = device->CreateDepthStencilView(m_Texture.Get(), &dsv, &m_Dsv);
        ThrowIfFailed(hr);
    }
}

void Texture2D::Load(ID3D11Device* device, ID3D11DeviceContext* context, const std::filesystem::path& path)
{
    ThrowIfFailed(CreateTextureFromFile(device, path, m_Texture.GetAddressOf(), m_Srv.GetAddressOf()));
    m_Texture->GetDesc(&m_Desc);
}

HRESULT Texture2D::CreateTextureFromFile(
    ID3D11Device* device,
    const std::filesystem::path& path,
    ID3D11Texture2D** texture,
    ID3D11ShaderResourceView** srv)
{
    HRESULT hr = 0;
    if (const auto ext = path.extension(); ext == ".bmp" || ext == ".png" || ext == ".jpg")
        hr = CreateWICTextureFromFile(device, path.wstring().c_str(),
            reinterpret_cast<ID3D11Resource**>(texture), srv);
    else if (ext == ".dds")
        hr = CreateDDSTextureFromFile(device, path.wstring().c_str(),
            reinterpret_cast<ID3D11Resource**>(texture), srv);
    return hr;
}

Texture2DArray::Texture2DArray(ID3D11Device* device, ID3D11DeviceContext* context, const std::filesystem::path& dir)
{
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        paths.push_back(entry.path());
    }

    CreateTextureArrayOnList(device, context, paths, m_Texture.Get(), m_Desc);
    Texture2DArray::CreateViews(device);
}

void Texture2DArray::CreateTextureArrayOnList(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    const std::vector<std::filesystem::path>& paths,
    ID3D11Texture2D* texture,
    D3D11_TEXTURE2D_DESC& desc)
{
    std::vector<ID3D11Texture2D*> tmp;
    tmp.reserve(paths.size());

    for (const auto& path : paths)
    {
        ID3D11Texture2D* tex = nullptr;
        ThrowIfFailed(CreateTextureFromFile(device, path, &tex, nullptr));
        tmp.emplace_back(tex);
    }

    D3D11_TEXTURE2D_DESC arrayDesc;
    tmp[0]->GetDesc(&arrayDesc);
    arrayDesc.MipLevels = 1;
    arrayDesc.ArraySize = static_cast<UINT>(tmp.size());
    arrayDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    ThrowIfFailed(device->CreateTexture2D(&arrayDesc, nullptr, &texture));

    for (UINT i = 0; i < tmp.size(); ++i)
        context->CopySubresourceRegion(
            texture, D3D11CalcSubresource(0, i, arrayDesc.MipLevels),
            0, 0, 0,
            tmp[i], D3D11CalcSubresource(0, 0, arrayDesc.MipLevels),
            nullptr);

    texture->GetDesc(&desc);
    for (const auto& tex : tmp)
        tex->Release();
}

Texture2DArray::Texture2DArray(
    ID3D11Device* device, ID3D11DeviceContext* context, const std::vector<std::filesystem::path>& paths)
{
    CreateTextureArrayOnList(device, context, paths, m_Texture.Get(), m_Desc);
    Texture2DArray::CreateViews(device);
}

void Texture2DArray::CreateViews(ID3D11Device* device)
{
    if (m_Srv == nullptr)
    {
        const auto srv = CD3D11_SHADER_RESOURCE_VIEW_DESC(m_Texture.Get(),
            D3D11_SRV_DIMENSION_TEXTURE2DARRAY, m_Desc.Format);
        const auto hr = device->CreateShaderResourceView(m_Texture.Get(), &srv, &m_Srv);
        ThrowIfFailed(hr);
    }
}
