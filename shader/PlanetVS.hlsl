#include "Planet.hlsli"

VertexOut main(uint vertexId : SV_VertexID)
{
    float2 gridOffset = float2(gridOffsetU, gridOffsetV) + float2(vertexId % 255, vertexId / 255) * gridSize;
    float3 cubeVertex = faceUp + gridOffset.x * faceRight + gridOffset.y * faceBottom;
    float3 unitSphere = normalize(cubeVertex);

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
    UBER_NOISE_FBM(geometryOctaves, unitSphere)

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
