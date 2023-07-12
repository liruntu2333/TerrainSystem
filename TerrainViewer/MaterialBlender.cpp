#define NOMINMAX

#include "MaterialBlender.h"
#include <xsimd/xsimd.hpp>

std::vector<uint32_t> AlbedoBlender::Blend(
    const int x, const int y, const unsigned w, const unsigned h, const unsigned mip) const
{
    using UintBatch = xsimd::batch<uint32_t, xsimd::default_arch>;
    using FloatBatch = xsimd::batch<float, xsimd::default_arch>;
    constexpr size_t batchSize = UintBatch::size;
    static_assert(SampleRatio % batchSize == 0);

    // const auto pc = points.size();
    const unsigned dw = w * SampleRatio;
    const unsigned dh = h * SampleRatio;
    std::vector<uint32_t> dst;
    dst.resize(dw * dh);

    const FloatBatch k(0.0f, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f);
    // each sample in weight map
    for (int wy = 0; wy < h; ++wy)
    {
        for (int wx = 0; wx < w; ++wx)
        {
            // one sample in weight map generates SampleRatio * SampleRatio pixels
            for (int row = 0; row < SampleRatio; ++row)
            {
                const auto w00 = m_Splat->GetVal(wx + x, wy + y, mip);
                const auto w10 = m_Splat->GetVal(wx + 1 + x, wy + y, mip);
                const auto w01 = m_Splat->GetVal(wx + x, wy + 1 + y, mip);
                const auto w11 = m_Splat->GetVal(wx + 1 + x, wy + 1 + y, mip);
                // use simd instructions to blend SampleRatio pixels in a row
                FloatBatch r(0.0f), g(0.0f), b(0.0f);
                // for each texture in atlas
                for (int layer = 0; layer < 4; ++layer)
                {
                    // trilinear interpolation
                    const float ty = 1.0f / SampleRatio * row;
                    const float w0 =
                        static_cast<float>(w00 >> layer * 8 & 0xFF) / 255.0f * (1.0f - ty) +
                        static_cast<float>(w01 >> layer * 8 & 0xFF) / 255.0f * ty;
                    const float w1 =
                        static_cast<float>(w10 >> layer * 8 & 0xFF) / 255.0f * (1.0f - ty) +
                        static_cast<float>(w11 >> layer * 8 & 0xFF) / 255.0f * ty;

                    const FloatBatch bw = ((1.0f - k) * w0 + k * w1);
                    const UintBatch src = UintBatch::load_aligned(&m_Atlas[layer]->GetVal(
                        (wx + x) * SampleRatio, (wy + y) * SampleRatio + row, mip));
                    const FloatBatch srcR = xsimd::batch_cast<float>(src & 0xFF) / 255.0f;
                    const FloatBatch srcG = xsimd::batch_cast<float>(src >> 8 & 0xFF) / 255.0f;
                    const FloatBatch srcB = xsimd::batch_cast<float>(src >> 16 & 0xFF) / 255.0f;
                    r += srcR * bw;
                    g += srcG * bw;
                    b += srcB * bw;
                }
                const UintBatch ur = xsimd::batch_cast<uint32_t>(r * 255);
                const UintBatch ug = xsimd::batch_cast<uint32_t>(g * 255);
                const UintBatch ub = xsimd::batch_cast<uint32_t>(b * 255);
                const auto c = ur | ug << 8 | ub << 16 | 0xFF000000;
                const size_t dy = wy * SampleRatio + row;
                const size_t dx = wx * SampleRatio;
                c.store_aligned(&dst[(dx + dy * dw)]);
            }
        }
    }

    return dst;
}
