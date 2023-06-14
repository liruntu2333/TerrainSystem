#include "ShaderUtil.hlsli"

cbuffer ObjectConstants : register(b1)
{
float4 g_ScaleFactor;
float4 g_Color;
}

float4 main(in float4 positionL : SV_POSITION) : SV_TARGET
{
    return g_Color;
}
