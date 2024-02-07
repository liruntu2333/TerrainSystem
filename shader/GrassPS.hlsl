#include "Grass.hlsli"
#include "ShaderUtil.hlsli"

float GetAo(float t)
{
    return lerp(0.6, 1.0, t);
}

float GetRoughness(float t)
{
    return lerp(0.5, 0.1, t);
}

Texture2D albedo : register(t0);
SamplerState anisotropicClamp : register(s0);

float4 main(VertexOut pin, bool frontFace : SV_IsFrontFace) : SV_TARGET
{
    float3 normal = normalize(pin.Normal);
    if (!frontFace) normal = -normal;

    float3 alb = albedo.Sample(anisotropicClamp, pin.Texture).rgb;
    if (debug)
    {
        alb = lerp(float3(1.0, 1.0, 0.0), float3(0.0, 1.0, 1.0), pin.Lod);
    }

    const float3 camDir   = normalize(camPos - pin.WldPos);
    float3 ambientDiffuse = Shade(normal, float3(0.0, 1.0, 0.0), 0.1, GetAo(pin.Texture.y));
    float3 h              = normalize(camDir + float3(0.0, 1.0, 0.0));
    float3 f0             = float3(0.04, 0.04, 0.04);
    float3 col            = Brdf(float3(0.0, 1.0, 0.0), camDir, normal, f0, alb, 0.0, GetRoughness(pin.Texture.y));
    return float4(col, 1.0f);
}
