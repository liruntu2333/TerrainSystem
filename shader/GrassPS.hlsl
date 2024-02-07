#include "Grass.hlsli"
#include "ShaderUtil.hlsli"

float GetAo(float t)
{
    return lerp(0.3, 0.97, t);
}

float GetRoughness(float t)
{
    return lerp(1, 0.1, t);
}

float GetTranslucency(float t)
{
    return lerp(0.0, 0.1, t);
}

Texture2D albedo : register(t0);
SamplerState anisotropicClamp : register(s0);

float4 main(VertexOut pin, bool frontFace : SV_IsFrontFace) : SV_TARGET
{
    float3 n = normalize(pin.Normal);
    if (!frontFace) n = -n;

    float3 alb = albedo.Sample(anisotropicClamp, pin.Texture).rgb;
    if (debug)
    {
        alb = lerp(float3(1.0, 1.0, 0.0), float3(0.0, 1.0, 1.0), pin.Lod);
    }
    const float3 l = float3(0.0, 1.0, 0.0);
    const float3 v = normalize(camPos - pin.WldPos);
    float t        = smoothstep(0.0, 1.0, pin.Texture.y);

    float3 col = Brdf(
        l,
        float3(0.4, 0.4, 0.4),
        v,
        n,
        alb,
        float3(0.04, 0.04, 0.04),
        0.0,
        GetRoughness(t),
        GetAo(t));
    col = ToneMapping(col);
    return float4(col, 1.0f);
}
