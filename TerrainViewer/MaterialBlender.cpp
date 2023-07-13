#define NOMINMAX
#define _XM_NO_INTRINSICS_ // in case of AVX-SSE transition penalty
#include "MaterialBlender.h"
#include <xsimd/xsimd.hpp>
#include <directxtk/SimpleMath.h>

namespace
{
    template <class T, class U>
    auto Lerp(const T& a, const T& b, const U t)
    {
        return a + (b - a) * t;
    }

    template <class T>
    void Normalize(T& x, T& y, T& z)
    {
        const auto m = rsqrt(x * x + y * y + z * z);
        x *= m;
        y *= m;
        z *= m;
    }

    struct Color
    {
        union
        {
            struct
            {
                uint8_t R, G, B, A;
            };

            uint8_t Vec[4];
            uint32_t Val;
        };

        [[nodiscard]] float GetChannel(const int i) const
        {
            constexpr auto m = 1 / 255.0f;
            return static_cast<float>(Vec[i]) * m;
        }

        template <int C>
        [[nodiscard]] float GetChannel() const
        {
            constexpr int channel = C;
            constexpr auto m = 1 / 255.0f;
            return static_cast<float>(Vec[channel]) * m;
        }

        Color() = default;
        ~Color() = default;
        operator uint32_t() const { return Val; }

        operator DirectX::SimpleMath::Vector3() const
        {
            return { static_cast<float>(R), static_cast<float>(G), static_cast<float>(B) };
        }
    };
}


std::vector<uint32_t> AlbedoBlender::Blend(
    const int x, const int y, const unsigned w, const unsigned h, const unsigned mip) const
{
    using UintBatch = xsimd::batch<uint32_t, xsimd::default_arch>;
    using FloatBatch = xsimd::batch<float, xsimd::default_arch>;
    constexpr size_t stride = UintBatch::size;
    static_assert(SampleRatio % stride == 0);

    // const auto pc = points.size();
    const unsigned dw = w * SampleRatio;
    const unsigned dh = h * SampleRatio;
    std::vector<uint32_t> dst;
    dst.resize(dw * dh);

    const FloatBatch t[] =
    {
        FloatBatch(1.0f / SampleRatio, 2.0f / SampleRatio, 3.0f / SampleRatio, 4.0f / SampleRatio,
            5.0f / SampleRatio, 6.0f / SampleRatio, 7.0f / SampleRatio, 8.0f / SampleRatio),
        FloatBatch(9.0f / SampleRatio, 10.0f / SampleRatio, 11.0f / SampleRatio, 12.0f / SampleRatio,
            13.0f / SampleRatio, 14.0f / SampleRatio, 15.0f / SampleRatio, 16.0f / SampleRatio),
    };
    // each sample in weight map
    for (int wy = 0; wy < h; ++wy)
    {
        for (int wx = 0; wx < w; ++wx)
        {
            // one sample in weight map generates SampleRatio * SampleRatio pixels
            for (int row = 0; row < SampleRatio; ++row)
            {
                auto w00 = reinterpret_cast<const Color&>(m_Splat->GetVal(wx + x, wy + y, mip));
                auto w10 = reinterpret_cast<const Color&>(m_Splat->GetVal(wx + x + 1, wy + y, mip));
                auto w01 = reinterpret_cast<const Color&>(m_Splat->GetVal(wx + x, wy + y + 1, mip));
                auto w11 = reinterpret_cast<const Color&>(m_Splat->GetVal(wx + x + 1, wy + y + 1, mip));

                for (int i = 0; i < SampleRatio / stride; ++i)
                {
                    // use simd intrinsics to blend SampleRatio pixels in a row
                    FloatBatch r(0.0f), g(0.0f), b(0.0f);
                    // for each texture in atlas
                    for (int l = 0; l < 4; ++l)
                    {
                        // trilinear interpolation
                        const float ty = 1.0f / SampleRatio * row;
                        const float w0 = Lerp(w00.GetChannel(l), w01.GetChannel(l), ty);
                        const float w1 = Lerp(w10.GetChannel(l), w11.GetChannel(l), ty);

                        const FloatBatch bw = Lerp(w0, w1, t[i]);
                        const UintBatch src = UintBatch::load_aligned(&m_Atlas[l]->GetVal(
                            (wx + x) * SampleRatio + stride * i, (wy + y) * SampleRatio + row, mip));
                        const FloatBatch srcR = xsimd::batch_cast<float>(src & 0xFF);
                        const FloatBatch srcG = xsimd::batch_cast<float>(src >> 8 & 0xFF);
                        const FloatBatch srcB = xsimd::batch_cast<float>(src >> 16 & 0xFF);
                        r += srcR * bw;
                        g += srcG * bw;
                        b += srcB * bw;
                    }
                    const auto c = xsimd::batch_cast<uint32_t>(r) |
                        xsimd::batch_cast<uint32_t>(g) << 8 |
                        xsimd::batch_cast<uint32_t>(b) << 16 |
                        0xFF000000;
                    const size_t dx = wx * SampleRatio + stride * i;
                    const size_t dy = wy * SampleRatio + row;
                    c.store_unaligned(&dst[(dx + dy * dw)]);
                }
            }
        }
    }

    return dst;
}

std::vector<uint32_t> NormalBlender::Blend(int splatX, int splatY, unsigned splatW, unsigned splatH, unsigned mip) const
{
    using DirectX::SimpleMath::Vector3;
    using UintBatch = xsimd::batch<uint32_t, xsimd::default_arch>;
    using FloatBatch = xsimd::batch<float, xsimd::default_arch>;
    constexpr size_t stride = UintBatch::size;
    static_assert(SampleRatio % UintBatch::size == 0);

    // const auto pc = points.size();
    const unsigned dw = splatW * SampleRatio;
    const unsigned dh = splatH * SampleRatio;
    std::vector<uint32_t> dst;
    dst.resize(dw * dh);

    const FloatBatch t[] =
    {
        FloatBatch(1.0f / SampleRatio, 2.0f / SampleRatio, 3.0f / SampleRatio, 4.0f / SampleRatio,
            5.0f / SampleRatio, 6.0f / SampleRatio, 7.0f / SampleRatio, 8.0f / SampleRatio),
        FloatBatch(9.0f / SampleRatio, 10.0f / SampleRatio, 11.0f / SampleRatio, 12.0f / SampleRatio,
            13.0f / SampleRatio, 14.0f / SampleRatio, 15.0f / SampleRatio, 16.0f / SampleRatio),
    };
    // each sample in weight map
    for (int wy = 0; wy < splatH; ++wy)
    {
        for (int wx = 0; wx < splatW; ++wx)
        {
            // one sample in weight map generates SampleRatio * SampleRatio pixels
            for (int row = 0; row < SampleRatio; ++row)
            {
                const float ty = 1.0f / SampleRatio * row;
                const auto w00 = reinterpret_cast<const Color&>(m_Splat->GetVal(wx + splatX, wy + splatY, mip));
                const auto w10 = reinterpret_cast<const Color&>(m_Splat->GetVal(wx + splatX + 1, wy + splatY, mip));
                const auto w01 = reinterpret_cast<const Color&>(m_Splat->GetVal(wx + splatX, wy + splatY + 1, mip));
                const auto w11 = reinterpret_cast<const Color&>(m_Splat->GetVal(wx + splatX + 1, wy + splatY + 1, mip));
                const auto b00 = reinterpret_cast<const Color&>(m_Base->GetVal(wx + splatX, wy + splatY, mip));
                const auto b10 = reinterpret_cast<const Color&>(m_Base->GetVal(wx + splatX + 1, wy + splatY, mip));
                const auto b01 = reinterpret_cast<const Color&>(m_Base->GetVal(wx + splatX, wy + splatY + 1, mip));
                const auto b11 = reinterpret_cast<const Color&>(m_Base->GetVal(wx + splatX + 1, wy + splatY + 1, mip));

                for (int i = 0; i < SampleRatio / stride; ++i)
                {
                    const Vector3 b0 = Vector3::Lerp(b00, b01, ty) * (2.0f / 255) - Vector3(1.0f);
                    const Vector3 b1 = Vector3::Lerp(b10, b11, ty) * (2.0f / 255) - Vector3(1.0f);
                    const FloatBatch xBase = Lerp(b0.x, b1.x, t[i]);
                    const FloatBatch yBase = Lerp(b0.y, b1.y, t[i]);
                    const FloatBatch zBase = Lerp(b0.z, b1.z, t[i]);

                    // use simd intrinsics to blend SampleRatio pixels in a row
                    // for each texture in atlas
                    FloatBatch x(0), y(0), z(0);
                    for (int l = 0; l < 4; ++l)
                    {
                        // trilinear interpolation
                        const float w0 = Lerp(w00.GetChannel(l), w01.GetChannel(l), ty);
                        const float w1 = Lerp(w10.GetChannel(l), w11.GetChannel(l), ty);

                        const FloatBatch wt = Lerp(w0, w1, t[i]) * 0.3f;
                        const UintBatch src = UintBatch::load_aligned(&m_Atlas[l]->GetVal(
                            (wx + splatX) * SampleRatio + i * stride, (wy + splatY) * SampleRatio + row, mip));
                        const FloatBatch srcX = xsimd::batch_cast<float>(src & 0xFF) * (2.0f / 255) - 1;
                        const FloatBatch srcY = xsimd::batch_cast<float>(src >> 8 & 0xFF) * (2.0f / 255) - 1;
                        const FloatBatch srcZ = xsimd::batch_cast<float>(src >> 16 & 0xFF) * (2.0f / 255) - 1;
                        x += srcX * wt;
                        y += srcY * wt;
                        z += srcZ * wt;
                    }
                    x = Lerp(xBase, x, 0.35f);
                    y = Lerp(yBase, y, 0.35f);
                    z = Lerp(zBase, y, 0.35f);
                    Normalize(x, y, z);
                    x = (x + 1) * (0.5f * 255);
                    y = (y + 1) * (0.5f * 255);
                    z = (z + 1) * (0.5f * 255);

                    const auto c =
                        xsimd::batch_cast<uint32_t>(x) |
                        xsimd::batch_cast<uint32_t>(y) << 8 |
                        xsimd::batch_cast<uint32_t>(z) << 16 |
                        0xFF000000;
                    const size_t dy = wy * SampleRatio + row;
                    const size_t dx = wx * SampleRatio + +i * stride;
                    c.store_unaligned(&dst[(dx + dy * dw)]);
                }
            }
        }
    }

    return dst;
}
