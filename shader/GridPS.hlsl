#include "ShaderUtil.hlsli"

SamplerState AnisotropicWrap : register(s0);
Texture2DArray Normal : register(t0);
Texture2DArray Albedo : register(t1);

void main(
    in float4 positionH : SV_POSITION,
    in float3 positionW : POSITION,
    in float4 colorIn : COLOR,
    in float3 texCoordF : TEXCOORD0,
    in float4 texCoordC : TEXCOORD1,

    out float4 colorOut : SV_TARGET)
{
    const float3 tcf = texCoordF;
    const float3 tcc = texCoordC.xyz;
    const float alpha = texCoordC.w;

    float4 nao = SampleClipmapLevel(Normal, AnisotropicWrap, tcf, tcc, alpha);
    const float3 normal = normalize(nao.rbg * 2 - 1);
    const float ao = nao.a;

    const float4 alr = SampleClipmapLevel(Albedo, AnisotropicWrap, tcf, tcc, alpha);
    const float3 albedo = alr.rgb;
    const float roughness = alr.a;
    static const float DielectricSpecular = 0.04f;

    const float3 view = normalize(ViewPosition - positionW);
    float3 r = reflect(-view, normal);
    const float3 f0 = float3(DielectricSpecular, DielectricSpecular, DielectricSpecular);
    const float3 nl = saturate(dot(normal, LightDirection));

    float3 col = 0.0f;
    col += Brdf(LightDirection, view, normal, f0, albedo, 0.0f, roughness) * LightIntensity * nl;
    col += AmbientIntensity * ao * albedo;

    //col = Shade(normal, LightDirection, LightIntensity, ao) * alr;
    col = ToneMapping(col);
    // col = GammaCorrect(col);

    colorOut = float4(col, 1.0f);
}
