#include "ShaderUtil.hlsli"

SamplerState g_AnisotropicClamp : register(s0);
Texture2D g_Normal : register(t0);
Texture2D g_Albedo : register(t1);

void main(
    in float4 positionL : SV_POSITION,
    in float4 colorIn : COLOR0,
    in float2 texCoord : TEXCOORD0,
    nointerpolation in float level : TEXCOORD1,

    out float4 colorOut : SV_TARGET
    )
{
	const float4 sample = g_Normal.SampleLevel(g_AnisotropicClamp, texCoord.xy, level);
	const float3 albedo = g_Albedo.SampleLevel(g_AnisotropicClamp, texCoord.xy, level).rgb;
    float3 normal = sample.rbg;
    normal = normalize(normal * 2.0f - 1.0f);
	float3 col = Shade(normal, g_SunDir, g_SunIntensity, sample.a, AMBIENT_INTENSITY)
        * albedo /** colorIn.rgb*/;

	colorOut = float4(col, 1.0f);
}
