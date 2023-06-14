#include "ShaderUtil.hlsli"

cbuffer ObjectConstants : register(b1)
{
uint2 g_PatchXy;
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
    const uint2 xy = (g_PatchXy + positionL) * PATCH_SCALE;
    const float2 uv = ((float2)xy + 0.5f) * texSz;
    const float h = g_Height.SampleLevel(g_PointClamp, uv, 0);
    const int2 localXy = g_PatchXy - g_CameraXy;
    float3 positionW = float3((positionL.x + localXy.x), h, (positionL.y + localXy.y));
    positionW *= float3(PATCH_SCALE, HEIGHTMAP_SCALE, PATCH_SCALE);

    // float z1 = g_Height.SampleLevel(g_PointClamp, uv + float2(-1, 0) * texSz, 0);
    // float z2 = g_Height.SampleLevel(g_PointClamp, uv + float2(+1, 0) * texSz, 0);
    // const float zx = z2 - z1;
    // z1 = g_Height.SampleLevel(g_PointClamp, uv + float2(0, -1) * texSz, 0);
    // z2 = g_Height.SampleLevel(g_PointClamp, uv + float2(0, +1) * texSz, 0);
    // const float zy = z2 - z1;
    // float3 normal = float3(-0.5f * HEIGHTMAP_SCALE * zx, 1.0f, -0.5f * HEIGHTMAP_SCALE * zy);
    // normal = normalize(normal);

	positionH = mul(float4(positionW, 1.0f), g_ViewProjectionLocal);
	texCoord = uv;
}
