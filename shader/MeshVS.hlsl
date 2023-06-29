#include "ShaderUtil.hlsli"

cbuffer ObjectConstants : register(b1)
{
int2 g_PatchXy;
uint g_PatchColor;
int g_Pad1[1];
}

SamplerState g_PointClamp : register(s0);
Texture2D<float> g_Height : register(t0);

void main(
    in float2 positionL : SV_Position,
    out float4 positionH : SV_Position,
    out float2 texCoord : TEXCOORD)
{
    float2 texSz;
    g_Height.GetDimensions(texSz.x, texSz.y);
    texSz = 1.0f / texSz;

    const float2 uv = ((positionL + g_PatchXy) * PatchScale + 0.5f) * texSz;
    const float h = g_Height.SampleLevel(g_PointClamp, uv, 0);

    float3 positionW = float3(
        positionL.x + g_PatchXy.x - ViewPatch.x,
        h,
        positionL.y + g_PatchXy.y - ViewPatch.y);
    positionW *= float3(PatchScale, HeightMapScale, PatchScale);
    positionW.y += 1000.0f;

    // compute normal
    // float z1 = g_Height.SampleLevel(g_PointClamp, uv + float2(-1, 0) * texSz, 0);
    // float z2 = g_Height.SampleLevel(g_PointClamp, uv + float2(+1, 0) * texSz, 0);
    // const float zx = z2 - z1;
    // z1 = g_Height.SampleLevel(g_PointClamp, uv + float2(0, -1) * texSz, 0);
    // z2 = g_Height.SampleLevel(g_PointClamp, uv + float2(0, +1) * texSz, 0);
    // const float zy = z2 - z1;
    // float3 normal = float3(-0.5f * HeightMapScale * zx, 1.0f, -0.5f * HeightMapScale * zy);
    // normal = normalize(normal);

    positionH = mul(float4(positionW, 1.0f), ViewProjectionLocal);
    texCoord = uv;
}
