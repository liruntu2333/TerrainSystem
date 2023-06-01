#include "ShaderUtil.hlsli"

SamplerState g_AnistropyClamp : register(s0);
Texture2D g_Normal : register(t0);

float4 main(VertexOut i) : SV_TARGET
{
	//const float3 normal = normalize(g_Normal.SampleLevel(g_AnistropyClamp, i.TexCoord, 0));
	float3 col = LoadColor(g_PatchColor).rgb;
	return float4(col, 1.0f);
}
