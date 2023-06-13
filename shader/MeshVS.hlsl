#include "ShaderUtil.hlsli"

SamplerState g_PointClamp : register(s0);
Texture2D<float> g_Height : register(t0);

VertexOut main(VertexIn i)
{
    VertexOut o;

    float2 texSz;
    g_Height.GetDimensions(texSz.x, texSz.y);
    texSz = 1.0f / texSz;
    const uint2 xy = (g_PatchXy + i.PositionL) * PATCH_SCALE;
    const float2 uv = ((float2)xy + 0.5f) * texSz;
    const float h = g_Height.SampleLevel(g_PointClamp, uv, 0);
    const int2 localXy = g_PatchXy - g_CameraXy;
    float3 posLow = float3((i.PositionL.x + localXy.x), h, (i.PositionL.y + localXy.y));
	posLow *= float3(PATCH_SCALE, HEIGHTMAP_SCALE, PATCH_SCALE);

    // float z1 = g_Height.SampleLevel(g_PointClamp, uv + float2(-1, 0) * texSz, 0);
    // float z2 = g_Height.SampleLevel(g_PointClamp, uv + float2(+1, 0) * texSz, 0);
    // const float zx = z2 - z1;
    // z1 = g_Height.SampleLevel(g_PointClamp, uv + float2(0, -1) * texSz, 0);
    // z2 = g_Height.SampleLevel(g_PointClamp, uv + float2(0, +1) * texSz, 0);
    // const float zy = z2 - z1;
    // float3 normal = float3(-0.5f * HEIGHTMAP_SCALE * zx, 1.0f, -0.5f * HEIGHTMAP_SCALE * zy);
    // normal = normalize(normal);

    o.PositionH = mul(float4(posLow, 1.0f), g_ViewProjection);
    o.TexCoord = uv;

    return o;
}
