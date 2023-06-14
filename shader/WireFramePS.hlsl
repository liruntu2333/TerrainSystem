#include "ShaderUtil.hlsli"

cbuffer ObjectConstants : register(b1)
{
uint2 g_PatchXy;
uint g_PatchColor;
int g_Pad1[1];
}

void main(out float4 color : SV_TARGET)
{
    float3 col = LoadColor(g_PatchColor).rgb;
    color = float4(col, 1.0f);
}
