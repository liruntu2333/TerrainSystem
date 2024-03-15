#include "Planet.hlsli"
#include "ShaderUtil.hlsli"
// #define LERP_ALTITUDE

Texture1D albedoRoughness : register(t0);
Texture1D f0Metallic : register(t1);
SamplerState pointClamp : register(s0);

float4 main(VertexOut pin) : SV_TARGET
{
    float pixDist = pin.NormalDist.w;
    // clip(pixDist + 0.01);
    float3 unitSphere = normalize(pin.NormalDist.xyz);

    float4 uberNoise = UberNoiseFbm(64, unitSphere);
    float sum        = uberNoise.w;
    float3 dSum      = uberNoise.xyz;

#ifdef LERP_ALTITUDE
    sum = pixDist;
#endif
    float dist = sum * elevation + radius;

    // https://math.stackexchange.com/questions/1071662/surface-normal-to-point-on-displaced-sphere
    float3 g = dSum / dist;
    float3 h = g - dot(g, unitSphere) * unitSphere;
    float3 N = normalize(unitSphere - elevation * h);

    N = normalize(mul(N, (float3x3)worldInvTrans));

    float3 L  = float3(0.0, 0.0, 1.0);
    float3 Li = float3(0.9568627, 0.9137255, 0.6078431);
    // float3 li = 0.0;

    float altitude  = sum * elevation;
    float3 worldPos = mul(float4(dist * unitSphere, 1.0f), world).xyz;
    float3 V        = normalize(camPos - worldPos);

    float u = sum / 2.0;
    // float u = sum;

    float4 albRough = albedoRoughness.SampleLevel(pointClamp, u, 0.0);
    // if (sum > 1.0 || sum < 0.0)
    // {
    //     albRough = float4(1, 0, 0, 1);
    // }
    float4 f0metal = f0Metallic.SampleLevel(pointClamp, u, 0.0);
    float3 alb     = albRough.rgb;
    // float3 alb = 1;
    // float3 alb = debugCol.xyz;
    // float3 alb = (N * 0.5 + 0.5);
    // float3 f0 = f0metal.rgb;
    float3 f0 = 0;
    // float metallic = f0metal.a;
    float metallic  = 0;
    float roughness = albRough.a;
    // float roughness = 1;
    float3 ami = 0.0;

    float3 color = Brdf(L, Li, V, N, alb, f0, metallic, roughness, ami);
    // float3 color = alb * EvalSh(n);
    // float3 color = pow(alb, 5);

    color = GammaCorrect(color);

    return float4(color, 1.0f);
}
