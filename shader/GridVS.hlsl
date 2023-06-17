#include "ShaderUtil.hlsli"

SamplerState g_PointWrap : register(s0);
Texture2D<float> g_Height : register(t0);

void main(
    in float2 positionL : SV_POSITION,
    in float4 scaleFactor : TEXCOORD0,
    in float4 texelFactor : TEXCOORD1,
    in float4 colorIn : COLOR0,

    out float4 positionH : SV_POSITION,
    out float4 colorOut : COLOR0
    )
{
    // convert from grid xy to world xy coordinates
    //  scaleFactor.xy: grid spacing of current level
    //  scaleFactor.zw: origin of current block within world
	const float2 worldPos = positionL * scaleFactor.xy + scaleFactor.zw;

    // compute coordinates for vertex texture
    //  texelFactor.xy: 1/(w, h) of texture
    //  texelFactor.zw: origin of block in texture
	const float2 uv = positionL * texelFactor.xy + texelFactor.zw;

    //  hf is elevation value in current (fine) level
    //  hc is elevation value in coarser level
	const float hf = g_Height.SampleLevel(g_PointWrap, uv, 0) * g_HeightMapScale;

    // compute alpha (transition parameter), and blend elevation.

	positionH = mul(float4(worldPos.x, hf, worldPos.y, 1), g_ViewProjection);
    colorOut = colorIn;
}
