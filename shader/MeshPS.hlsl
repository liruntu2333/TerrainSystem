#include "ShaderUtil.hlsli"

SamplerState g_AnisotropicClamp : register(s0);
Texture2D g_Normal : register(t0);
Texture2D g_Albedo : register(t1);

cbuffer ObjectConstants : register(b1)
{
uint2 g_PatchXy;
uint g_PatchColor;
int g_Pad1[1];
}

void main(
    in float4 positionH : SV_Position,
    in float2 texCoord : TEXCOORD,
    out float4 color : SV_TARGET)
{
    float2 normXz = g_Normal.Sample(g_AnisotropicClamp, texCoord.xy).rg;
	normXz = normXz * 2.0 - 1.0;
	float normY = sqrt(1.0 - dot(normXz, normXz));
	const float3 norm = normalize(float3(normXz.x, normY, normXz.y));

	const float3 albedo = g_Albedo.Sample(g_AnisotropicClamp, texCoord.xy).rgb;

    float3 col = Shade(norm, g_LightDirection, g_LightIntensity, 1, AMBIENT_INTENSITY)
        * albedo /** LoadColor(g_PatchColor).rgb*/;
    color = float4(col, 1.0f);
}
