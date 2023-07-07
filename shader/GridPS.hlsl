#include "ShaderUtil.hlsli"

SamplerState g_LinearClamp : register(s0);
Texture2D g_Normal : register(t0);
Texture2D g_Albedo : register(t1);

void main(
    in float4 positionH : SV_POSITION,
    in float4 colorIn : COLOR,
    in float3 texCoord : TEXCOORD0,

    out float4 colorOut : SV_TARGET)
{
    // const float2 pnf = g_Normal.Sample(g_LinearClamp, texCoord.xy, texCoord.z).rg;
    // const float2 pnc = g_Normal.SampleLevel(g_LinearClamp, texCoord.xy, texCoord.z + 1).rg;
    float4 noc = g_Normal.Sample(g_LinearClamp, texCoord.xy);
    // float2 pn = g_Normal.Sample(g_LinearClamp, texCoord.xy).rg;
    const float3 n = normalize(noc.rbg * 2 - 1);
    const float oc = noc.a;

    // const float3 alf = g_Albedo.SampleLevel(g_LinearClamp, texCoord.xy, texCoord.z).rgb;
    // const float3 alc = g_Albedo.SampleLevel(g_LinearClamp, texCoord.xy, texCoord.z + 1).rgb;
    const float3 al = g_Albedo.Sample(g_LinearClamp, texCoord.xy).rgb;
    // const float3 al = g_Albedo.Sample(g_LinearClamp, texCoord.xy).rgb;

    float3 col = Shade(n, LightDirection, LightIntensity, oc, Ambient) * al
        /** colorIn.rgb*/;
    col = ToneMapping(col);
	col = GammaCorrect(col);

    colorOut = float4(col, 1.0f);
}
