#include "Planet.hlsli"

cbuffer cb1 : register(b1)
{
    float4 vertex[64 + 1][8];
}

float4 main(uint vi : SV_VertexID, uint ii : SV_InstanceID) : SV_POSITION
{
    float4 pos = vertex[ii][vi];
    return mul(pos, viewProj);
}
