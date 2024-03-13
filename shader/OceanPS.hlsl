#include "Planet.hlsli"
#include "ShaderUtil.hlsli"

float4 main(VertexOut pin) : SV_TARGET
{
    float3 unitSphere = normalize(pin.NormalDist.xyz);

    UBER_NOISE_FBM(geometryOctaves, unitSphere)
	float3 alb = lerp(float3(0.0, 0.2, 1.0), float3(0.662745118f, 0.662745118f, 0.662745118f), abs(sum - oceanLevel) * 2.0);

    float3 L        = float3(0.0, 0.0, 1.0);
    float3 Li       = float3(0.9568627, 0.9137255, 0.6078431);
    float metallic  = 0.0;
    float f0        = 0.0;
    float roughness = 0.5;
    float3 ami      = 0.0;
    float3 N        = unitSphere;
    float3 V        = normalize(camPos - mul(float4(N * radius, 1.0f), world).xyz);

    float3 col = Brdf(L, Li, V, N, alb, f0, metallic, roughness, ami);

    return float4(col, 1);
}
