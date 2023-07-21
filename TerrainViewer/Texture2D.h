#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include  <filesystem>
#include <queue>

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

        struct Rectangle
        {
            unsigned X {}, Y {}, W {}, H {};

            Rectangle(const unsigned x, const unsigned y, const unsigned w, const unsigned h)
                : X(x), Y(y), W(w), H(h) { }

            Rectangle operator*(const unsigned s) const
            {
                return { X * s, Y * s, W * s, H * s };
            }

            [[nodiscard]] bool IsEmpty() const { return W == 0 || H == 0; }
        };

        template <class T>
        struct UpdateArea
        {
            Rectangle Rect;
            std::vector<T> Src {};

            UpdateArea(const Rectangle& rect, std::vector<T> src) : Rect(rect), Src(src) { }
        };

        // [[nodiscard]] std::unique_ptr<MapGuard> Map(
        //     ID3D11DeviceContext* context,
        //     unsigned int subresource,
        //     D3D11_MAP mapType,
        //     unsigned int mapFlags);

        template <class T>
        void UpdateToroidal(
            ID3D11DeviceContext* context, unsigned arraySlice, const UpdateArea<T>& area);

        void GenerateMips(ID3D11DeviceContext* context);

    protected:
        bool m_IsDirty = true;
    };

    template <class T>
    void ClipmapTexture::UpdateToroidal(
        ID3D11DeviceContext* context, const unsigned arraySlice, const UpdateArea<T>& area)
    {
        // TODO : support other block compression formats
        const bool isBc3 = m_Desc.Format == DXGI_FORMAT_BC3_UNORM;
        constexpr size_t bc3BlockSize = 16;
        std::queue<UpdateArea<T>> jobs;
        jobs.push(area);
        const auto texSz = m_Desc.Width;
        const auto subresource = D3D11CalcSubresource(0, arraySlice, m_Desc.MipLevels);
        while (!jobs.empty())
        {
            auto& job = jobs.front();
            auto& [x, y, w, h] = job.Rect;
            auto& src = job.Src;
            // cut into parts when warp cross edges
            if (x + w > texSz)
            {
                const unsigned w1 = texSz - x;
                const unsigned w2 = w - w1;
                std::vector<T> src1(w1 * h);
                std::vector<T> src2(w2 * h);
                if (isBc3)
                {
                    const size_t pitch1 = (w1 >> 2) * bc3BlockSize;
                    const size_t pitch2 = (w2 >> 2) * bc3BlockSize;
                    const size_t srcPitch = (w >> 2) * bc3BlockSize;
                    for (unsigned i = 0; i < h >> 2; ++i)
                    {
                        std::copy_n(src.begin() + i * srcPitch, pitch1, src1.begin() + i * pitch1);
                        std::copy_n(src.begin() + i * srcPitch + pitch1, pitch2, src2.begin() + i * pitch2);
                    }
                }
                else
                {
                    for (unsigned i = 0; i < h; ++i)
                    {
                        std::copy_n(src.begin() + i * w, w1, src1.begin() + i * w1);
                        std::copy_n(src.begin() + i * w + w1, w2, src2.begin() + i * w2);
                    }
                }
                jobs.push(UpdateArea<T>(Rectangle(x, y, w1, h), std::move(src1)));
                jobs.push(UpdateArea<T>(Rectangle(0, y, w2, h), std::move(src2)));
            }
            else if (y + h > texSz)
            {
                const unsigned h1 = texSz - y;
                const unsigned h2 = h - h1;
                std::vector<T> src2(w * h2);
                std::copy_n(src.begin() + w * h1, w * h2, src2.begin());
                src.resize(w * h1);
                jobs.push(UpdateArea<T>(Rectangle(x, y, w, h1), std::move(src)));
                jobs.push(UpdateArea<T>(Rectangle(x, 0, w, h2), std::move(src2)));
            }
            else
            {
                const UINT rowPitch = isBc3 ? bc3BlockSize * (w >> 2) : w * sizeof(T);
                CD3D11_BOX box(x, y, 0, x + w, y + h, 1);
                context->UpdateSubresource(GetTexture(), subresource,
                    &box, src.data(), rowPitch, 0);
            }
            jobs.pop();
        }

        m_IsDirty = true;
    }
}
