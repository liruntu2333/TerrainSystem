#include "Planet.hlsli"

VertexOut main(float3 unitSphere : SV_Position)
{
    // float4 noised;
    // float4 sum = 0.0;
    // float amp = 0.5;
    // float freq = 1.0;
    // for (int i = 0; i < normalOctaves; i++)
    // {
    // 	float4 d = Hubris::DerivateNoise(unitSphere * freq);
    // 	sum += amp * d;
    // 	// sum.yzw += amp * d.yzw;
    // 	// sum.x += amp * d.x / (1.0 + dot(d.yzw, d.yzw));
    // 	freq *= lacunarity;
    // 	amp *= gain;
    // }
    // noised = sum;
    float sum   = 0.0;
    float3 dSum = 0.0;
    float amp   = 1.0;
    float freq  = 1.0;

    for (int i = 0; i < geometryOctaves; i++)
    {
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

    float dist = radius + sum * elevation;

    float3 position = unitSphere * dist;

    // float3 g = noiD.yzw / dist;
    // float3 h = g - dot(g, unitSphere) * unitSphere;
    // float3 n = normalize(unitSphere - elevation * h);

    VertexOut vout;
    vout.Position   = mul(float4(position, 1.0f), worldViewProj);
    vout.NormalDist = float4(unitSphere, sum);
    return vout;
}
