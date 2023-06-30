#include "ShaderUtil.hlsli"

SamplerState PointWrap : register(s0);
Texture2DArray<float> Height : register(t0);

void main(
    in uint2 positionLf : SV_POSITION,   // local position in footprint at finer level
    in float4 wldParams : TEXCOORD0,
    in float4 texParams : TEXCOORD1,
    in uint4 lvlParams : TEXCOORD2,

    out float4 positionH : SV_POSITION,
    out float4 color : COLOR,
    out float3 texCoord : TEXCOORD0    // coordinates for normal & albedo lookup
    )
{
    // degenrate triangles into coarser level
    // lvlParams.xy: offset of current footprint in level local space
    const uint2 positionLc = (positionLf + lvlParams.xy) % 2 ? positionLf + 1 : positionLf;

    // convert grid xy to world xz coordinates
    // wldParams.xy: grid spacing of current level
    // wldParams.zw: origin of current footprint in world space
    const float2 pf = positionLf * wldParams.xy + wldParams.zw;
    const float2 pc = positionLc * wldParams.xy + wldParams.zw;

    // compute alpha based on view distance for temporal continuity
	float2 alpha = saturate((abs(pf - ViewPosition.xz) / wldParams.xy - AlphaOffset) * OneOverWidth);
    alpha.x = max(alpha.x, alpha.y);

    // blend position xz for space continuity to avoid T-junctions and popping
    float2 xz = lerp(pf, pc, alpha.x);

    // compute coordinates for vertex texture
    // texParams.xy: origin of footprint in fine texture
    // texParams.zw: origin of footprint in coarse texture
    static const float SampleRateFine = 1.0f / 256.0f;
    static const float SampleRateCoarse = 0.5f / 256.0f;
    const float2 uvf = positionLf * SampleRateFine + texParams.xy;
    const float2 uvc = positionLc * SampleRateCoarse + texParams.zw;
    const float2 uv = lerp(uvf, uvc, alpha.x);

    // lvlParams.w : level
    const float w = lvlParams.w + alpha.x;

    // blend elevation value
    const float hf = Height.SampleLevel(PointWrap, float3(uvf, lvlParams.w), 0);
    const float hc = Height.SampleLevel(PointWrap, float3(uvc, lvlParams.w + 1), 0);
    float h = lerp(hf, hc, alpha.x);
    h *= HeightMapScale;

    const float3 positionW = float3(xz.x, h, xz.y);
    positionH = float4(positionW, 1);
    positionH = mul(positionH, ViewProjection);
    color = LoadColor(lvlParams.z); // lvlParams.z : color
	texCoord = float3(xz / 8192, w);
}
