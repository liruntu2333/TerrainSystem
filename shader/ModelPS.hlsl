#include "ShaderUtil.hlsli"

struct VertexOut
{
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL;
    float2 Texture : TEXCOORD0;
    float3 WldPos : TEXCOORD1;
};

cbuffer ModelConstants : register(b0)
{
    float4x4 worldViewProjection;
    float4x4 viewProjection;
    float4x4 world;
    float3 camPos;
    float pad;
}

Texture2D albedo : register(t0);
SamplerState anisotropicClamp : register(s0);

float4 main(VertexOut pin) : SV_TARGET
{
    float3 n       = normalize(pin.Normal);
    const float3 l = float3(0.0, 1.0, 0.0);
    const float3 v = normalize(camPos - pin.WldPos);
    float t        = smoothstep(0.0, 1.0, pin.Texture.y);
    float3 alb     = albedo.Sample(anisotropicClamp, pin.Texture).rgb;

    float3 col = Brdf(
        l,
        float3(0.4, 0.4, 0.4),
        v,
        n,
        alb,
        float3(0.04, 0.04, 0.04),
        0.0,
        1.0,
        0.3);
    col = ToneMapping(col);
    return float4(col, 1.0f);
}
