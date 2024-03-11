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

    float sum   = 0.0;
    float3 dSum = 0.0;
    float amp   = 1.0;
    float freq  = 1.0;

    [unroll(32)]
    for (int i = 0; ; i++)
    {
        float3 v = unitSphere * freq;
        if (any(ddx(v) > 0.70710678) || any(ddy(v) > 0.70710678))
        {
            break;
        }

        float4 dNoise  = SimplexNoiseGrad(unitSphere * freq + dSum * perturb);
        float4 billowy = Billowy(dNoise);
        float4 ridged  = Ridged(dNoise);

        dNoise = lerp(dNoise, ridged, max(0.0, sharpness));
        dNoise = lerp(dNoise, billowy, abs(min(0.0, sharpness)));

        float3 dSumErosion = dSum + dNoise.xyz * amp;
        float sumErosion   = sum + dNoise.w * amp;

        float erosion = lerp(1.0, 1.0 / (1.0 + dot(dSumErosion, dSumErosion)), slopeErosion);
        erosion *= lerp(1.0, 1.0 - smoothstep(0.0, 1.0, abs(sumErosion)), altitudeErosion);

        sum += amp * erosion * dNoise.w;
        dSum += amp * erosion * dNoise.xyz;

        freq *= lacunarity;
        amp *= gain;
    }

#ifdef LERP_ALTITUDE
    sum = pixDist;
#endif
    float dist = sum * elevation + radius; // radius + sum * elevation;

    // https://math.stackexchange.com/questions/1071662/surface-normal-to-point-on-displaced-sphere
    float3 g = dSum / (radius + sum * elevation);
    float3 h = g - dot(g, unitSphere) * unitSphere;
    float3 N = normalize(unitSphere - elevation * h);

    N = normalize(mul(N, (float3x3)worldInvTrans));

    float3 L  = float3(0.0, 0.0, 1.0);
    float3 Li = float3(0.9568627, 0.9137255, 0.6078431);
    // float3 li = 0.0;

    float altitude  = (sum * elevation + 50.0);
    float3 worldPos = unitSphere * dist; //mul(float4(dist * unitSphere, 1.0f), worldInvTrans).xyz;
    float3 V        = normalize(camPos - worldPos);

    float u = abs(unitSphere.y) + altitude / 650.0;
    // float u = sum;

    float4 albRough = albedoRoughness.Sample(pointClamp, u);
    float4 f0metal  = f0Metallic.Sample(pointClamp, u);
    float3 alb      = albRough.rgb;
    // float3 alb = 1;
    // float3 alb = debugCol.xyz;
    // float3 alb = (N * 0.5 + 0.5).z;
    float3 f0 = f0metal.rgb;

    // float3 f0 = 0;
    float metallic = f0metal.a;
    // float metallic = 0;
    float roughness = albRough.a;
    // float roughness = 1;
    float3 ami = 0.0;

    float3 color = Brdf(L, Li, V, N, alb, f0, metallic, roughness, ami);
    // float3 color = alb * EvalSh(n);
    // float3 color = pow(alb, 5);

    color = GammaCorrect(color);

    return float4(color, 1.0f);
}
