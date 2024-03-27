#include "Planet.hlsli"

VertexOut main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    Instance ins      = instances[instanceId];
    uint face         = ins.faceOctaves & 0xff;
    uint octaves      = (ins.faceOctaves >> 8) & 0xff;
	float2 gridOffset = ins.faceOffset + float2(vertexId % 129, vertexId / 129) * ins.gridSize;
    float3 cubeVertex = GetCubeVertex(face, gridOffset);
    float3 unitSphere = normalize(cubeVertex);

    float3 position = unitSphere * radius;

    VertexOut vout;
    vout.Position    = mul(float4(position, 1.0f), worldViewProj);
    vout.WorldPosSum = float4(unitSphere, 0.0);
    vout.Normal      = unitSphere;
    vout.Octaves     = octaves;
    return vout;
}
