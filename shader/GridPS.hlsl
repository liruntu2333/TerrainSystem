#include "ShaderUtil.hlsli"

SamplerState g_LinearClamp : register(s0);
Texture2D<float2> g_Normal : register(t0);
Texture2D g_Albedo : register(t1);

void main(
    in float4 positionH : SV_POSITION,
    in float4 colorIn : COLOR,
    in float3 texCoord : TEXCOORD,

    out float4 colorOut : SV_TARGET
    )
{
    // const float2 pnf = g_Normal.Sample(g_LinearClamp, texCoord.xy, texCoord.z).rg;
    // const float2 pnc = g_Normal.SampleLevel(g_LinearClamp, texCoord.xy, texCoord.z + 1).rg;
	float2 pn = g_Normal.SampleLevel(g_LinearClamp, texCoord.xy, texCoord.z).rg;
    pn = pn * 2 - 1;
    const float3 n = float3(pn.x, sqrt(1.0 - dot(pn, pn)), pn.y);

    // const float3 alf = g_Albedo.SampleLevel(g_LinearClamp, texCoord.xy, texCoord.z).rgb;
    // const float3 alc = g_Albedo.SampleLevel(g_LinearClamp, texCoord.xy, texCoord.z + 1).rgb;
	const float3 al = g_Albedo.SampleLevel(g_LinearClamp, texCoord.xy, texCoord.z).rgb;

	float3 col = Shade(n, g_LightDirection, g_LightIntensity, 1, AMBIENT_INTENSITY) * al
        /** colorIn.rgb*/;

    colorOut = float4(col, 1.0f);
}
