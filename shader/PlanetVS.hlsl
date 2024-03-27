#include "Planet.hlsli"

VertexOut main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    Instance ins      = instances[instanceId];
    uint face         = ins.faceOctaves & 0xff;
    uint octaves      = (ins.faceOctaves >> 8) & 0xff;
	float2 gridOffset = ins.faceOffset + float2(vertexId % 129, vertexId / 129) * ins.gridSize;
    float3 cubeVertex = GetCubeVertex(face, gridOffset);
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
    float4 gradNoise = UberNoiseFbm(unitSphere, octaves);

    float dist = radius + gradNoise.w * elevation;
    // // https://math.stackexchange.com/questions/1071662/surface-normal-to-point-on-displaced-sphere
    // float3 g = gradNoise.xyz / dist;
    // float3 h = g - dot(g, unitSphere) * unitSphere;
    // float3 N = normalize(unitSphere - elevation  * h);
    //
    // N = normalize(mul(N, (float3x3)worldInvTrans));

    float3 position = unitSphere * dist;

    VertexOut vout;
    vout.Position    = mul(float4(position, 1.0f), worldViewProj);
    vout.WorldPosSum = float4(mul(float4(position, 1.0f), world).xyz, gradNoise.w);
    vout.Normal      = unitSphere;
    vout.Octaves     = octaves;
    return vout;
}
