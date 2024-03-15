#include "Planet.hlsli"

VertexOut main(uint vertexId : SV_VertexID)
{
    float2 gridOffset = float2(gridOffsetU, gridOffsetV) + float2(vertexId % 255, vertexId / 255) * gridSize;
    float3 cubeVertex = faceUp + gridOffset.x * faceRight + gridOffset.y * faceBottom;
    float3 unitSphere = normalize(cubeVertex);

    float3 position = unitSphere * radius;

    VertexOut vout;
    vout.Position   = mul(float4(position, 1.0f), worldViewProj);
    vout.NormalDist = float4(unitSphere, 0.0);
    return vout;
}
