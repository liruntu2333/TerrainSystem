#include "ShaderUtil.hlsli"

SamplerState g_PointWrap : register(s0);
Texture2D<float> g_Height : register(t0);

void main(
    in float2 positionL : SV_POSITION,
    in float2 gridScale : TEXCOORD0,
    in float2 gridOffset : TEXCOORD1,
    in float2 texelScale : TEXCOORD2,
    in float2 textureOffset : TEXCOORD3,
    in float4 colorIn : COLOR0,

    out float4 positionH : SV_POSITION,
    out float4 colorOut : COLOR0
    )
{
    // convert from grid xy to world xy coordinates
    //  GridScale: grid spacing of current level
    //  GridOffset: origin of current block within world
    const float2 worldPos = positionL * gridScale + gridOffset;

    // compute coordinates for vertex texture
    //  TexelScale: 1/(w, h) of texture
    //  TextureOffset: origin of block in texture 
    const float2 uv = positionL * texelScale + textureOffset;
    // const float2 uv = (worldPos + 0.5f) / 8192.0f + 0.5f;

    const float mipFin = log2(gridScale.x);
    const float mipCoarse = mipFin + 1.0f;
    //  hf is elevation value in current (fine) level
    //  hc is elevation value in coarser level
	const float hf = g_Height.SampleLevel(g_PointWrap, worldPos / 8192.0f + 0.5f, 0) * HEIGHTMAP_SCALE;

    // compute alpha (transition parameter), and blend elevation.

	positionH = mul(float4(worldPos.x, hf, worldPos.y, 1), g_ViewProjection);
    colorOut = colorIn;
}
