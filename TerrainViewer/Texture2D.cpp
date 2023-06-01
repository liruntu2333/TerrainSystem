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
	HRESULT hr;
	if (path.extension().string() != ".dds")
	{
		hr = CreateWICTextureFromFile(device, path.wstring().c_str(),
			reinterpret_cast<ID3D11Resource**>(m_Texture.GetAddressOf()), m_Srv.GetAddressOf());
	}
	else
	{
		hr = CreateDDSTextureFromFile(device, path.wstring().c_str(),
			reinterpret_cast<ID3D11Resource**>(m_Texture.GetAddressOf()), m_Srv.GetAddressOf());
	}

	ThrowIfFailed(hr);

	m_Texture->GetDesc(&m_Desc);
}

Texture2D::Texture2D(
	ID3D11Device* device, const D3D11_TEXTURE2D_DESC& desc, const void* initData, uint32_t systemPitch) :
	m_Desc(desc)
{
	const D3D11_SUBRESOURCE_DATA data{ initData, systemPitch, 0 };
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
	const auto hr = CreateWICTextureFromFile(device, path.wstring().c_str(),
		reinterpret_cast<ID3D11Resource**>(m_Texture.ReleaseAndGetAddressOf()),
		m_Srv.ReleaseAndGetAddressOf());

	ThrowIfFailed(hr);

	m_Texture->GetDesc(&m_Desc);

	CreateViews(device);
}

Texture2DArray::Texture2DArray(ID3D11Device* device, ID3D11DeviceContext* context, const std::filesystem::path& dir)
{
	std::vector<std::filesystem::path> paths;
	for (const auto& entry : std::filesystem::directory_iterator(dir))
	{
		paths.push_back(entry.path());
	}
	std::vector<ID3D11Texture2D*> textures;
	textures.reserve(paths.size());
	for (const auto& path : paths)
	{
		ID3D11Texture2D* texture = nullptr;
		if (path.extension() == ".dds")
			ThrowIfFailed(CreateDDSTextureFromFile(device, path.c_str(),
				reinterpret_cast<ID3D11Resource**>(&texture), nullptr));
		else
			ThrowIfFailed(CreateWICTextureFromFile(device, path.c_str(),
				reinterpret_cast<ID3D11Resource**>(&texture), nullptr));

		textures.emplace_back(texture);
	}

	D3D11_TEXTURE2D_DESC arrayDesc;
	textures[0]->GetDesc(&arrayDesc);
	arrayDesc.ArraySize = static_cast<UINT>(textures.size());
	arrayDesc.MipLevels = -1;
	arrayDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	arrayDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
	const auto hr = device->CreateTexture2D(&arrayDesc, nullptr, &m_Texture);
	ThrowIfFailed(hr);
	for (UINT i = 0; i < arrayDesc.ArraySize; ++i)
	{
		context->CopySubresourceRegion(m_Texture.Get(),
			D3D11CalcSubresource(0, i, arrayDesc.MipLevels),
			0, 0, 0, textures[i], 0, nullptr);
	}
	m_Texture->GetDesc(&m_Desc);
	Texture2DArray::CreateViews(device);
	context->GenerateMips(m_Srv.Get());

	for (const auto& texture : textures)
	{
		texture->Release();
	}
}

Texture2DArray::Texture2DArray(
	ID3D11Device* device, ID3D11DeviceContext* context, const std::vector<std::filesystem::path>& paths)
{
	std::vector<ID3D11Texture2D*> textures;
	textures.reserve(paths.size());

	for (const auto& path : paths)
	{
		ID3D11Texture2D* texture = nullptr;
		if (path.extension() == ".dds")
			ThrowIfFailed(CreateDDSTextureFromFile(device, path.c_str(),
				reinterpret_cast<ID3D11Resource**>(&texture), nullptr));
		else
			ThrowIfFailed(CreateWICTextureFromFile(device, path.c_str(),
				reinterpret_cast<ID3D11Resource**>(&texture), nullptr));

		textures.emplace_back(texture);
	}

	D3D11_TEXTURE2D_DESC arrayDesc;
	textures[0]->GetDesc(&arrayDesc);
	arrayDesc.MipLevels = 1;
	arrayDesc.ArraySize = static_cast<UINT>(textures.size());
	arrayDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	ThrowIfFailed(device->CreateTexture2D(&arrayDesc, nullptr, &m_Texture));
	for (UINT i = 0; i < textures.size(); ++i)
	{
		context->CopySubresourceRegion(
			m_Texture.Get(), D3D11CalcSubresource(0, i, arrayDesc.MipLevels),
			0, 0, 0,
			textures[i], D3D11CalcSubresource(0, 0, arrayDesc.MipLevels),
			nullptr);
	}
	m_Texture->GetDesc(&m_Desc);
	Texture2DArray::CreateViews(device);

	for (const auto& texture : textures)
	{
		texture->Release();
	}
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

void Texture2DArray::Load(ID3D11Device* device, ID3D11DeviceContext* context, const std::filesystem::path& dir)
{
	std::vector<std::filesystem::path> paths;
	for (const auto& entry : std::filesystem::directory_iterator(dir))
	{
		paths.push_back(entry.path());
	}
	std::sort(paths.begin(), paths.end());
	std::vector<ID3D11Texture2D*> textures;
	for (const auto& path : paths)
	{
		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
		const auto hr = CreateWICTextureFromFile(device, path.c_str(),
			reinterpret_cast<ID3D11Resource**>(texture.GetAddressOf()), nullptr);
		ThrowIfFailed(hr);
		textures.push_back(texture.Get());
	}

	const auto hr = device->CreateTexture2D(&m_Desc, nullptr, m_Texture.ReleaseAndGetAddressOf());
	ThrowIfFailed(hr);
	for (UINT i = 0; i < m_Desc.ArraySize; ++i)
	{
		context->CopySubresourceRegion(m_Texture.Get(), i, 0, 0, 0,
			textures[i], 0, nullptr);
	}
}