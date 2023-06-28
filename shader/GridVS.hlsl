#include "ShaderUtil.hlsli"

SamplerState g_PointWrap : register(s0);
Texture2DArray<float> g_Height : register(t0);

void main(
    in uint2 positionL : SV_POSITION,   // local position in footprint at finer level
    in float4 scales : TEXCOORD0,
    in float4 offsets : TEXCOORD1,
    in uint4 params : TEXCOORD2,

    out float4 positionH : SV_POSITION,
    out float4 color : COLOR,
    out float3 texCoord : TEXCOORD    // coordinates for normal & albedo lookup
    )
{
    // compute approximated alpha (transition parameter) based on grid index
    float2 alpha = saturate((abs(positionL + params.xy - 127) - g_AlphaOffset) * g_OneOverWidth);
    alpha.x = max(alpha.x, alpha.y);

    // degenrate triangles into coarser level
    // params.xy: offset of current footprint in level local space
    const uint2 positionC = uint2(positionL + params.xy) % 2 ? positionL + 1 : positionL;

    // convert grid xy to world xz coordinates
    // scales.x: grid spacing of current level, y : texel spacing of current level
    // offsets.xy: origin of current footprint in world space
    const float2 pf = positionL * scales.xy + offsets.xy;
    const float2 pc = positionC * scales.xy + offsets.xy;
    float2 p = lerp(pf, pc, alpha.x);

    // recompute alpha based on view distance for temporal continuity
    alpha = saturate((abs(p - g_ViewPosition) / scales.xy - g_AlphaOffset) * g_OneOverWidth);
    alpha.x = max(alpha.x, alpha.y);

    // blend position xz for space continuity to avoid T-junctions and popping
    p = lerp(pf, pc, alpha.x);

    // compute coordinates for vertex texture
    // offsets.zw: origin of footprint in texture
	const float2 uvf = positionL * scales.zw + offsets.zw;
	const float2 uvc = positionC * scales.zw + offsets.zw;
    const float2 uv = lerp(uvf, uvc, alpha.x);

    // params.w : level 
    const float w = params.w + alpha.x;

    // blend elevation value
	const float hf = g_Height.SampleLevel(g_PointWrap, float3(uvf, params.w), 0);
	const float hc = g_Height.SampleLevel(g_PointWrap, float3(uvc, params.w + 1), 0);
	float h = hf;
    h *= g_HeightMapScale;

    float3 positionW = float3(p.x, h, p.y);
    positionH = mul(float4(positionW, 1), g_ViewProjection);
    color = LoadColor(params.z); // params.z : color
    texCoord = float3(uv, w);
}
