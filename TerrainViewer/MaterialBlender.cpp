#define NOMINMAX
#define _XM_NO_INTRINSICS_ // in case of AVX-SSE transition penalty
#include "MaterialBlender.h"
#include <xsimd/xsimd.hpp>
#include <directxtk/SimpleMath.h>

using DirectX::SimpleMath::Vector4;
using UintBatch = xsimd::batch<uint32_t, xsimd::default_arch>;
using FloatBatch = xsimd::batch<float, xsimd::default_arch>;

namespace
{
    constexpr size_t Stride = UintBatch::size;
    constexpr float M = 1.0f / 255.0f;
    const FloatBatch T[] =
    {
        FloatBatch(0.0f / NormalBlender::SampleRatio,
            1.0f / NormalBlender::SampleRatio, 2.0f / NormalBlender::SampleRatio,
            3.0f / NormalBlender::SampleRatio, 4.0f / NormalBlender::SampleRatio,
            5.0f / NormalBlender::SampleRatio, 6.0f / NormalBlender::SampleRatio,
            7.0f / NormalBlender::SampleRatio),
        FloatBatch(8.0f / NormalBlender::SampleRatio,
            9.0f / NormalBlender::SampleRatio, 10.0f / NormalBlender::SampleRatio,
            11.0f / NormalBlender::SampleRatio, 12.0f / NormalBlender::SampleRatio,
            13.0f / NormalBlender::SampleRatio, 14.0f / NormalBlender::SampleRatio,
            15.0f / NormalBlender::SampleRatio),
    };

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
            return static_cast<float>(Vec[i]) * M;
        }

        operator uint32_t() const { return Val; }

        operator DirectX::SimpleMath::Vector4() const
        {
            return { static_cast<float>(R), static_cast<float>(G), static_cast<float>(B), static_cast<float>(A) };
        }
    };

    void Linear(
        const FloatBatch& baseX, const FloatBatch& baseY, const FloatBatch& baseZ,
        FloatBatch& x, FloatBatch& y, FloatBatch& z, const float t)
    {
        x = Lerp(baseX, x, t);
        y = Lerp(baseY, y, t);
        z = Lerp(baseZ, z, t);
        x = x * (2.0f / 255) + -1.0f;
        y = y * (2.0f / 255) + -1.0f;
        z = z * (2.0f / 255) + -1.0f;
    }

    void PartialDerivative(
        FloatBatch& xBase, FloatBatch& yBase, FloatBatch& zBase,
        FloatBatch& x, FloatBatch& y, FloatBatch& z, const float t)
    {
        xBase = xBase * (2.0f / 255) + -1;
        yBase = yBase * (2.0f / 255) + -1;
        zBase = zBase * (2.0f / 255) + -1;
        x = x * (2.0f / 255) + -1;
        y = y * (2.0f / 255) + -1;
        z = z * (2.0f / 255) + -1;
        const auto rbz = reciprocal(zBase);
        const auto rz = reciprocal(z);
        x = Lerp(xBase * rbz, x * rz, t);
        y = Lerp(yBase * rbz, y * rz, t);
        z = 1;
    }

    void Whiteout(
        FloatBatch& xBase, FloatBatch& yBase, FloatBatch& zBase,
        FloatBatch& x, FloatBatch& y, FloatBatch& z)
    {
        xBase = xBase * (2.0f / 255) + -1;
        yBase = yBase * (2.0f / 255) + -1;
        zBase = zBase * (2.0f / 255) + -1;

        x = x * (2.0f / 255) + -1;
        y = y * (2.0f / 255) + -1;
        z = z * (2.0f / 255) + -1;

        x += xBase;
        y += yBase;
        z *= zBase;
    }

    void UnrealDeveloperNetwork(
        FloatBatch& xBase, FloatBatch& yBase, FloatBatch& zBase,
        FloatBatch& x, FloatBatch& y, FloatBatch& z)
    {
        xBase = xBase * (2.0f / 255) + -1;
        yBase = yBase * (2.0f / 255) + -1;
        zBase = zBase * (2.0f / 255) + -1;
        x = x * (2.0f / 255) + -1;
        y = y * (2.0f / 255) + -1;
        z = z * (2.0f / 255) + -1;

        x += xBase;//Lerp(xBase * 2, x * 2, t);
        y += yBase;//Lerp(yBase * 2, x * 2, t);
        z = zBase;
    }

    void Reorient(
        FloatBatch& xBase, FloatBatch& yBase, FloatBatch& zBase,
        FloatBatch& x, FloatBatch& y, FloatBatch& z)
    {
        xBase = xBase * (2.0f / 255) + -1;
        yBase = yBase * (2.0f / 255) + -1;
        zBase = zBase * (2.0f / 255);
        x = x * (-2.0f / 255) + 1;
        y = y * (-2.0f / 255) + 1;
        z = z * (2.0f / 255) + -1;
        const auto dot = (x * xBase + y * yBase + z * zBase) * reciprocal(zBase);
        x = xBase * dot - x;
        y = yBase * dot - y;
        z = zBase * dot - z;
    }

    void Unity(
        FloatBatch& xBase, FloatBatch& yBase, FloatBatch& zBase,
        FloatBatch& x, FloatBatch& y, FloatBatch& z)
    {
        xBase = xBase * (2.0f / 255) + -1;
        yBase = yBase * (2.0f / 255) + -1;
        zBase = zBase * (2.0f / 255) + -1;
        x = x * (2.0f / 255) + -1;
        y = y * (2.0f / 255) + -1;
        z = z * (2.0f / 255) + -1;

        const auto tx = x * zBase + y * xBase + z * xBase;
        const auto ty = x * yBase + y * zBase + z * yBase;
        const auto tz = -x * xBase - y * yBase + z * zBase;
        x = tx;
        y = ty;
        z = tz;
    }
}

std::vector<uint32_t> AlbedoBlender::Blend(
    const int splatX, const int splatY, const unsigned splatW, const unsigned splatH, const unsigned mip) const
{
    // const auto pc = points.size();
    const unsigned dw = splatW * SampleRatio;
    const unsigned dh = splatH * SampleRatio;
    std::vector<uint32_t> dst;
    dst.resize(dw * dh);

    // each sample in weight map
    for (int wy = 0; wy < splatH; ++wy)
    {
        for (int wx = 0; wx < splatW; ++wx)
        {
            // one sample in weight map generates SampleRatio * SampleRatio pixels
            for (int row = 0; row < SampleRatio; ++row)
            {
                auto w00 = reinterpret_cast<const Color&>(m_Splat->GetVal(wx + splatX, wy + splatY, mip));
                auto w10 = reinterpret_cast<const Color&>(m_Splat->GetVal(wx + splatX + 1, wy + splatY, mip));
                auto w01 = reinterpret_cast<const Color&>(m_Splat->GetVal(wx + splatX, wy + splatY + 1, mip));
                auto w11 = reinterpret_cast<const Color&>(m_Splat->GetVal(wx + splatX + 1, wy + splatY + 1, mip));

                for (int i = 0; i < SampleRatio / Stride; ++i)
                {
                    // use simd intrinsics to blend SampleRatio pixels in a row
                    FloatBatch r(0.0f), g(0.0f), b(0.0f), a(0.0f);
                    // for each texture in atlas
                    for (int l = 0; l < 4; ++l)
                    {
                        // trilinear interpolation
                        const float ty = 1.0f / SampleRatio * row;
                        const float w0 = Lerp(w00.GetChannel(l), w01.GetChannel(l), ty);
                        const float w1 = Lerp(w10.GetChannel(l), w11.GetChannel(l), ty);

                        const auto wt = Lerp(w0, w1, T[i]);
                        const UintBatch src = UintBatch::load_aligned(&m_Atlas[l]->GetVal(
                            (wx + splatX) * SampleRatio + Stride * i, (wy + splatY) * SampleRatio + row, mip));
                        const FloatBatch srcR = xsimd::batch_cast<float>(src & 0xFF);
                        const FloatBatch srcG = xsimd::batch_cast<float>(src >> 8 & 0xFF);
                        const FloatBatch srcB = xsimd::batch_cast<float>(src >> 16 & 0xFF);
                        const FloatBatch srcA = xsimd::batch_cast<float>(src >> 24 & 0xFF);
                        r += srcR * wt;
                        g += srcG * wt;
                        b += srcB * wt;
                        a += srcA * wt;
                    }
                    const auto c =
                        xsimd::batch_cast<uint32_t>(r + 0.01f) |
                        xsimd::batch_cast<uint32_t>(g + 0.01f) << 8 |
                        xsimd::batch_cast<uint32_t>(b + 0.01f) << 16 |
                        xsimd::batch_cast<uint32_t>(a + 0.01f) << 24;
                    const size_t dx = wx * SampleRatio + Stride * i;
                    const size_t dy = wy * SampleRatio + row;
                    c.store_unaligned(&dst[(dx + dy * dw)]);
                }
            }
        }
    }

    return dst;
}

std::vector<uint32_t> NormalBlender::Blend(
    int splatX, int splatY, unsigned splatW, unsigned splatH, unsigned mip,
    BlendMethod method) const
{
    // const auto pc = points.size();
    const unsigned dw = splatW * SampleRatio;
    const unsigned dh = splatH * SampleRatio;
    std::vector<uint32_t> dst;
    dst.resize(dw * dh);

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

                for (int i = 0; i < SampleRatio / Stride; ++i)
                {
                    const Vector4 b0 = Vector4::Lerp(b00, b01, ty);
                    const Vector4 b1 = Vector4::Lerp(b10, b11, ty);

                    FloatBatch xBase = Lerp(b0.x, b1.x, T[i]);
                    FloatBatch yBase = Lerp(b0.y, b1.y, T[i]);
                    FloatBatch zBase = Lerp(b0.z, b1.z, T[i]);
                    FloatBatch wBase = Lerp(b0.w, b1.w, T[i]);

                    // use simd intrinsics to blend SampleRatio pixels in a row
                    // for each texture in atlas
                    FloatBatch x(0), y(0), z(0), w(0);
                    for (int l = 0; l < 4; ++l)
                    {
                        // trilinear interpolation
                        const float w0 = Lerp(w00.GetChannel(l), w01.GetChannel(l), ty);
                        const float w1 = Lerp(w10.GetChannel(l), w11.GetChannel(l), ty);

                        const auto wt = Lerp(w0, w1, T[i]);
                        const UintBatch src = UintBatch::load_aligned(&m_Detail[l]->GetVal(
                            (wx + splatX) * SampleRatio + i * Stride, (wy + splatY) * SampleRatio + row, mip));
                        const FloatBatch srcX = xsimd::batch_cast<float>(src & 0xFF);
                        const FloatBatch srcY = xsimd::batch_cast<float>(src >> 8 & 0xFF);
                        const FloatBatch srcZ = xsimd::batch_cast<float>(src >> 16 & 0xFF);
                        const FloatBatch srcW = xsimd::batch_cast<float>(src >> 24 & 0xFF);
                        x += srcX * wt;
                        y += srcY * wt;
                        z += srcZ * wt;
                        w += srcW * wt;
                    }

                    switch (method)
                    {
                    case BaseOnly: ::Linear(xBase, yBase, zBase, x, y, z, 0);
                        break;
                    case DetailOnly: ::Linear(xBase, yBase, zBase, x, y, z, 1);
                        break;
                    case PartialDerivative: ::PartialDerivative(xBase, yBase, zBase, x, y, z, 0.5f);
                        break;
                    case Linear: ::Linear(xBase, yBase, zBase, x, y, z, 0.5f);
                        break;
                    case Whiteout: ::Whiteout(xBase, yBase, zBase, x, y, z);
                        break;
                    case Unreal: ::UnrealDeveloperNetwork(xBase, yBase, zBase, x, y, z);
                        break;
                    case Reorient: ::Reorient(xBase, yBase, zBase, x, y, z);
                        break;
                    case Unity: ::Unity(xBase, yBase, zBase, x, y, z);
                        break;
                    }

                    Normalize(x, y, z);
                    x = (x + 1) * (0.5f * 255);
                    y = (y + 1) * (0.5f * 255);
                    z = (z + 1) * (0.5f * 255);

                    // blend ao
                    w *= wBase * (1.0f / 255);

                    const auto c =
                        xsimd::batch_cast<uint32_t>(x + 0.01f) |
                        xsimd::batch_cast<uint32_t>(y + 0.01f) << 8 |
                        xsimd::batch_cast<uint32_t>(z + 0.01f) << 16 |
                        xsimd::batch_cast<uint32_t>(w + 0.01f) << 24;
                    const size_t dy = wy * SampleRatio + row;
                    const size_t dx = wx * SampleRatio + i * Stride;
                    c.store_unaligned(&dst[(dx + dy * dw)]);
                }
            }
        }
    }

    return dst;
}
