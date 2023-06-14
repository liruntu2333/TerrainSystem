#include "ShaderUtil.hlsli"

cbuffer ObjectConstants : register(b1)
{
float4 g_ScaleFactor;
float4 g_Color;
}

void main(
    in float2 positionL : SV_POSITION,
    out float4 positionH : SV_POSITION)
{
    // convert from grid xy to world xy coordinates
    //  ScaleFactor.xy: grid spacing of current level
    //  ScaleFactor.zw: origin of current block within world
    float2 worldPos = positionL * g_ScaleFactor.xy + g_ScaleFactor.zw;
    positionH = mul(float4(worldPos.x, 0, worldPos.y, 1), g_ViewProjection);
}
