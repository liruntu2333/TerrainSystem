#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include  <filesystem>
#include <directxtk/DirectXHelpers.h>

namespace DirectX
{
    class Texture2D
    {
    public:
        Texture2D(ID3D11Device* device, const D3D11_TEXTURE2D_DESC& desc);
        Texture2D(ID3D11Device* device, const std::filesystem::path& path);
        Texture2D(ID3D11Device* device, const D3D11_TEXTURE2D_DESC& desc, const void* initData, uint32_t systemPitch);
        Texture2D() = default;
        virtual ~Texture2D() = default;

        Texture2D(Texture2D&&) = default;
        Texture2D& operator=(Texture2D&&) = default;
        Texture2D(const Texture2D&) = delete;
        Texture2D& operator=(const Texture2D&) = delete;

        [[nodiscard]] auto GetTexture() const { return m_Texture.Get(); }
        explicit operator ID3D11Texture2D*() const { return m_Texture.Get(); }
        //explicit operator ID3D11Resource*() const { return m_HTex.GetHeightClip(); }
        [[nodiscard]] auto GetSrv() const { return m_Srv.Get(); }
        [[nodiscard]] auto GetRtv() const { return m_Rtv.Get(); }
        [[nodiscard]] auto GetUav() const { return m_Uav.Get(); }
        [[nodiscard]] auto GetDsv() const { return m_Dsv.Get(); }
        [[nodiscard]] const auto& GetDesc() const { return m_Desc; }

        virtual void CreateViews(ID3D11Device* device);
        virtual void Load(
            ID3D11Device* device,
            ID3D11DeviceContext* context,
            const std::filesystem::path& path);

    protected:
        static HRESULT CreateTextureFromFile(
            ID3D11Device* device,
            const std::filesystem::path& path,
            ID3D11Texture2D** texture,
            ID3D11ShaderResourceView** srv);

        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_Texture = nullptr;

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_Srv = nullptr;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_Rtv = nullptr;
        Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_Uav = nullptr;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_Dsv = nullptr;

        D3D11_TEXTURE2D_DESC m_Desc {};
    };

    class Texture2DArray : public Texture2D
    {
    public:
        Texture2DArray(
            ID3D11Device* device,
            ID3D11DeviceContext* context,
            const std::filesystem::path& dir);
        Texture2DArray(
            ID3D11Device* device,
            ID3D11DeviceContext* context,
            const std::vector<std::filesystem::path>& paths);
        Texture2DArray(
            ID3D11Device* device,
            const CD3D11_TEXTURE2D_DESC& desc);

        static void CreateTextureArrayOnList(
            ID3D11Device* device,
            ID3D11DeviceContext* context,
            const std::vector<std::filesystem::path>& paths,
            ID3D11Texture2D* texture,
            D3D11_TEXTURE2D_DESC& desc);

        ~Texture2DArray() override = default;

        void CreateViews(ID3D11Device* device) override;
    };

    class ClipmapTexture : public Texture2DArray
    {
    public:
        ClipmapTexture(ID3D11Device* device, const CD3D11_TEXTURE2D_DESC& desc);
        ~ClipmapTexture() override = default;

        void CreateViews(ID3D11Device* device) override;

        [[nodiscard]] std::unique_ptr<MapGuard> Map(
            ID3D11DeviceContext* context,
            unsigned int subresource,
            D3D11_MAP mapType,
            unsigned int mapFlags);
    };
}
