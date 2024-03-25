#include "Planet.hlsli"

VertexOut main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    Instance ins      = instances[instanceId];
    float2 gridOffset = float2(ins.gridOffsetX, ins.gridOffsetY) + float2(vertexId % 255, vertexId / 255) * ins.gridSize;
    float3 cubeVertex = ins.faceForward + gridOffset.x * ins.faceRight + gridOffset.y * ins.faceUp;
    float3 unitSphere = normalize(cubeVertex);

    float3 position = unitSphere * radius;

    VertexOut vout;
    vout.Position    = mul(float4(position, 1.0f), worldViewProj);
    vout.WorldPosSum = float4(unitSphere, 0.0);
    vout.Normal      = unitSphere;
    return vout;
}
