#define NOMINMAX

#include "MaterialBlender.h"
#include <xsimd/xsimd.hpp>

using namespace DirectX;

std::vector<uint32_t> AlbedoBlender::Blend(
    const int x, const int y, const unsigned w, const unsigned h, const unsigned mip) const
{
    using UintBatch = xsimd::batch<uint32_t, xsimd::default_arch>;
    constexpr size_t batchSize = UintBatch::size;
    static_assert(SampleRatio % batchSize == 0);

    // const auto pc = points.size();
    const unsigned dw = w * SampleRatio;
    const unsigned dh = h * SampleRatio;
    std::vector<uint32_t> dst;
    dst.resize(dw * dh);

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
                UintBatch r(0), g(0), b(0);

                // for each texture in atlas
                for (int layer = 0; layer < 4; ++layer)
                {
                    // trilinear interpolation
                    const uint8_t w0 = ((w00 >> layer * 8 & 0xFF) * (SampleRatio - row) +
                        (w01 >> layer * 8 & 0xFF) * row) >> 3;
                    const uint8_t w1 = ((w10 >> layer * 8 & 0xFF) * (SampleRatio - row) +
                        (w11 >> layer * 8 & 0xFF) * row) >> 3;
                    const UintBatch k(0, 1, 2, 3, 4, 5, 6, 7);
                    const auto alpha = ((SampleRatio - k) * w0 + k * w1) >> 3;
                    const UintBatch src = UintBatch::load_aligned(&m_Atlas[layer]->GetVal(
                        (wx + x) * SampleRatio, (wy + y) * SampleRatio + row, mip));
                    r += (src & 0xFF) * alpha;
                    g += (src >> 8 & 0xFF) * alpha;
                    b += (src >> 16 & 0xFF) * alpha;
                }
                r = r >> 8 & 0xFF;
                g = g >> 8 & 0xFF;
                b = b >> 8 & 0xFF;
                const auto c = r | g << 8 | b << 16 | 0xFF000000;
                const size_t dy = wy * SampleRatio + row;
                const size_t dx = wx * SampleRatio;
                c.store_aligned(&dst[(dx + dy * dw)]);
            }
        }
    }

    return dst;
}
