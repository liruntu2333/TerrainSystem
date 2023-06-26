#include "ShaderUtil.hlsli"

SamplerState g_PointWrap : register(s0);
Texture2D<float> g_Height : register(t0);

void main(
    in uint2 positionL : SV_POSITION,   // local position in footprint at finer level
    in float4 localOffset : TEXCOORD0,
    in float4 levelOffset : TEXCOORD1,
    in uint2 colorLevel : TEXCOORD2,

    out float4 positionH : SV_POSITION,
    out float4 color : COLOR,
    out float3 texCoord : TEXCOORD    // coordinates for normal & albedo lookup
    )
{
    // compute approximated alpha (transition parameter) based on grid index
    float2 alpha = saturate((abs(positionL + localOffset.zw - 127) - g_AlphaOffset) * g_OneOverWidth);
    alpha.x = max(alpha.x, alpha.y);

    // degenrate triangles into coarser level
    // localOffset.zw: offset of current footprint in level local space
    const uint2 gridC = uint2(positionL + localOffset.zw) % 2 ? positionL + 1 : positionL;

    // convert grid xy to world xz coordinates
    // localOffset.x: grid spacing of current level, y : texel spacing of current level
    // levelOffset.xy: origin of current footprint in world space
    const float2 pf = positionL * localOffset.x + levelOffset.xy;
    const float2 pc = gridC * localOffset.x + levelOffset.xy;
    float2 p = lerp(pf, pc, alpha.x);

    // recompute alpha based on view distance for temporal continuity
    alpha = saturate((abs(p - g_ViewPosition) / localOffset.x - g_AlphaOffset) * g_OneOverWidth);
    alpha.x = max(alpha.x, alpha.y);

    // blend position xz for space continuity to avoid T-junctions and popping
    p = lerp(pf, pc, alpha.x);

    // compute coordinates for vertex texture
    // levelOffset.zw: origin of footprint in texture
    const float2 uvf = pf * localOffset.y / localOffset.x + 0.5f;
    const float2 uvc = pc * localOffset.y / localOffset.x + 0.5f;
    const float2 uv = lerp(uvf, uvc, alpha.x);

	const float w = colorLevel.y + alpha.x;

    // blend elevation value
    // colorLevel.y : level 
    const float hf = g_Height.SampleLevel(g_PointWrap, uvf, colorLevel.y);
    const float hc = g_Height.SampleLevel(g_PointWrap, uvc, colorLevel.y + 1);
    float h = lerp(hf, hc, alpha.x);
    h *= g_HeightMapScale;

    float3 positionW = float3(p.x, h, p.y);
    positionH = mul(float4(positionW, 1), g_ViewProjection);
    color = LoadColor(colorLevel.x); // colorLevel.x : color
    texCoord = float3(uv, w);
}
