#include "ShaderUtil.hlsli"

SamplerState LinearClamp : register(s0);
Texture2D Normal : register(t0);
Texture2DArray Albedo : register(t1);

void main(
    in float4 positionH : SV_POSITION,
    in float4 colorIn : COLOR,
    in float3 texCoordF : TEXCOORD0,
    in float4 texCoordC : TEXCOORD1,

    out float4 colorOut : SV_TARGET)
{
    // const float2 pnf = Normal.Sample(LinearClamp, texCoordF.xy, texCoordF.z).rg;
    // const float2 pnc = Normal.SampleLevel(LinearClamp, texCoordF.xy, texCoordF.z + 1).rg;
    float4 noc = Normal.Sample(LinearClamp, texCoordF.xy);
    // float2 pn = Normal.Sample(LinearClamp, texCoordF.xy).rg;
    const float3 n = normalize(noc.rbg * 2 - 1);
    const float oc = noc.a;

    const float3 al = SampleClipmapLevel(Albedo, LinearClamp, texCoordF, texCoordC.xyz,
        texCoordC.w).rgb;
    // const float3 al = Albedo.Sample(LinearClamp, texCoordF.xy).rgb;

    float3 col = Shade(float3(0, 1, 0), LightDirection, LightIntensity, oc, Ambient) * al
        * colorIn.rgb;
    col = ToneMapping(col);
    col = GammaCorrect(col);

    colorOut = float4(col, 1.0f);
}
