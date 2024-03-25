#include "Planet.hlsli"
#include "ShaderUtil.hlsli"
#define OCEAN_FOAM

float4 main(VertexOut pin) : SV_TARGET
{
    float3 unitSphere = normalize(pin.Normal.xyz);

#ifdef OCEAN_FOAM
    float sum  = UberNoiseFbm(unitSphere).w;
    float3 alb = lerp(SHALLOW_OCEAN_COLOR, DEEP_OCEAN_COLOR, smoothstep(0.0, 1.0, saturate((oceanLevel - sum) * 30.0)));
#else
    float3 alb = DEEP_OCEAN_COLOR;
#endif
    // float trans = lerp(0.2, 1.0, smoothstep(0.0, 1.0, saturate((oceanLevel - sum) * 5.0)));
    float trans = 0.5;

    float3 L        = float3(0.0, 0.0, 1.0);
    float3 Li       = float3(0.9568627, 0.9137255, 0.6078431);
    float metallic  = 0.0;
    float f0        = 0.0;
    float roughness = 0.1;
    float3 ami      = 0.0;
    float3 N        = mul(unitSphere, (float3x3)worldInvTrans).xyz;
    float3 V        = normalize(camPos - mul(float4(N * radius, 1.0f), world).xyz);

    float3 col = Brdf(L, Li, V, N, alb, f0, metallic, roughness, ami);

    col = GammaCorrect(col);

    return float4(col, trans);
}
