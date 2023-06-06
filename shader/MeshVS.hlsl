#include "ShaderUtil.hlsli"

SamplerState g_PointClamp : register(s0);
Texture2D<float> g_Height : register(t0);

VertexOut main(VertexIn i)
{
    VertexOut o;

    float2 texSz;
    g_Height.GetDimensions(texSz.x, texSz.y);
    const float2 uvOffset = 0.5f / texSz;
    const uint2 xy = (g_PatchXy + i.PositionL) * PATCH_SIZE;
    const float2 uv = (float2)xy / texSz + uvOffset;

    float h = g_Height.SampleLevel(g_PointClamp, uv, 0) * HEIGHTMAP_SCALE;
    const float3 posLow = float3(
        (i.PositionL.x + (int)g_PatchXy.x - g_CameraXy.x) * PATCH_SIZE,
        h,
        (i.PositionL.y + (int)g_PatchXy.y - g_CameraXy.y) * PATCH_SIZE);

    // Deprecated code for morphing between low and high detail, topology changes while iterating.
    // To preserve topology need to set restraint on TIN construction process.
    //
    //const float t = saturate((length(g_CameraPosition - posLow) - 20.0f) / 30.0f);
    //const float2 xy = lerp(i.PositionL.xy, i.PositionL.zw, t);

    //float h2 = g_Height.SampleLevel(g_PointClamp, xy, 0.0f) * 2000.0f;
    //const float3 posHigh = float3(i.PositionL.z * 255.0f, h2, i.PositionL.w * 255.0f);
    //const float3 posW = lerp(posLow, posHigh, t);

    o.PositionH = mul(float4(posLow, 1.0f), g_ViewProjection);
    o.TexCoord = uv;

    return o;
}
