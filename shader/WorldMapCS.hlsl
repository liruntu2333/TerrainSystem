#include "Planet.hlsli"
#include "ShaderUtil.hlsli"

RWTexture2D<float4> worldMap : register(u0);

Texture1D albedoRoughness : register(t0);
SamplerState pointClamp : register(s0);

[numthreads(16, 16, 1)]
void main(uint3 dtId : SV_DispatchThreadID)
{
    float2 uv = (float2(dtId.xy) + 0.5f) / float2(512.0, 256.0);

    float longitude = (uv.x * 2.0 - 1.0) * Pi;
    float latitude  = -(uv.y * 2.0 - 1.0) * (Pi / 2.0);

    float3 unitSphere = normalize(
        float3(
            sin(longitude) * cos(latitude),
            sin(latitude),
            cos(longitude) * cos(latitude)));

    float sum = UberNoiseFbm(unitSphere).w;

	// float u = sum * 0.5;
    // float3 alb = albedoRoughness.SampleLevel(pointClamp, u, 0.0).rgb;
	float3 alb = sum * 0.5 + 0.5;
    if (sum < oceanLevel)
    {
        alb = lerp(DEEP_OCEAN_COLOR, alb, OCEAN_ALPHA);
    }

    // alb = GammaCorrect(alb);

    worldMap[dtId.xy] = float4(alb, 1.0);
}
