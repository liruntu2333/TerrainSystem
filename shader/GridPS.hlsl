#include "ShaderUtil.hlsli"

SamplerState LinearClamp : register(s0);
Texture2DArray Normal : register(t0);
Texture2DArray Albedo : register(t1);

void main(
    in float4 positionH : SV_POSITION,
    in float4 colorIn : COLOR,
    in float3 texCoordF : TEXCOORD0,
    in float4 texCoordC : TEXCOORD1,

    out float4 colorOut : SV_TARGET)
{
    const float3 tcf = texCoordF;
    const float3 tcc = texCoordC.xyz;
	const float alpha = texCoordC.w;
    // const float2 pnf = Normal.Sample(LinearClamp, texCoordF.xy, texCoordF.z).rg;
    // const float2 pnc = Normal.SampleLevel(LinearClamp, texCoordF.xy, texCoordF.z + 1).rg;
    float3 n = SampleClipmapLevel(Normal, LinearClamp, tcf, tcc, alpha).rbg;
    // float2 pn = Normal.Sample(LinearClamp, texCoordF.xy).rg;
    n = normalize(n * 2 - 1);

    const float3 al = SampleClipmapLevel(Albedo, LinearClamp, tcf, tcc, alpha).rgb;
    // const float3 al = Albedo.Sample(LinearClamp, texCoordF.xy).rgb;

    float3 col = Shade(n, LightDirection, LightIntensity, 1.0f, Ambient) * al;
    col = ToneMapping(col);
    col = GammaCorrect(col);

    colorOut = float4(col, 1.0f);
}
