#include "ShaderUtil.hlsli"

SamplerState g_PointWrap : register(s0);
Texture2D<float> g_Height : register(t0);

void main(
    in uint2 positionL : SV_POSITION,
    in float4 localOffset : TEXCOORD0,
    in float4 levelOffset : TEXCOORD1,
    in uint2 colorLevel : TEXCOORD2,

    out float4 positionH : SV_POSITION,
    out float4 color : COLOR,
    out float3 texCoord : TEXCOORD    // coordinates for normal & albedo lookup
    )
{
    // degenrate perimeter triangles to avoid T-junction
    // localOffset.zw: offset of current footprint in level local space
    uint2 xyL = uint2(positionL + localOffset.zw) % 254;
    if (xyL.x == 0 && xyL.y % 2) positionL.y++;
    if (xyL.y == 0 && xyL.x % 2) positionL.x++;

    // convert from grid xy to world xy coordinates
    // localOffset.x: grid spacing of current level, y : texel spacing of current level
    // levelOffset.xy: origin of current footprint in world space
    const float2 positionW = positionL * localOffset.x + levelOffset.xy;

    // compute coordinates for vertex texture
    // levelOffset.zw: origin of footprint in texture
	const float2 uv = positionW * localOffset.y / localOffset.x;

    // compute alpha (transition parameter), and blend elevation.
    float2 alpha = saturate((abs(positionL + localOffset.zw - 127) - g_AlphaOffset)
        * g_OneOverWidth);
    alpha.x = max(alpha.x, alpha.y);

    // colorLevel.y : level 
    const float hf = g_Height.SampleLevel(g_PointWrap, uv, colorLevel.y);
    const float hc = g_Height.SampleLevel(g_PointWrap, uv, colorLevel.y + 1);
	float h = lerp(hf, hc, floor(alpha.x));
    h *= g_HeightMapScale;

    const float w = colorLevel.y + alpha.x;

	positionH = mul(float4(positionW.x, h, positionW.y, 1), g_ViewProjection);
    color = LoadColor(colorLevel.x); // colorLevel.x : color
    texCoord = float3(uv, w);
}
