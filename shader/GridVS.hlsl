#include "ShaderUtil.hlsli"

static const float SampleRateFine = 1.0f / 256.0f;
static const float SampleRateCoarse = 0.5f / 256.0f;

SamplerState PointWrap : register(s0);
Texture2DArray<float> Height : register(t0);

void main(
    in uint2 positionLf : SV_POSITION,   // local position in footprint at finer level
    in float4 wldParams : TEXCOORD0,
    in float4 texParams : TEXCOORD1,
    in uint4 lvlParams : TEXCOORD2,

    out float4 positionH : SV_POSITION,
    out float3 positionW : POSITION,
    out float4 color : COLOR,
    out float3 texCoordF : TEXCOORD0,
    out float4 texCoordC : TEXCOORD1
    )
{
    // degenrate triangles into coarser level
    // lvlParams.xy: offset of current footprint in level local space
    const uint2 positionLc = (positionLf + lvlParams.xy) % 2
                                 ? positionLf + 1
                                 : positionLf;

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
    float3 uvf = float3(positionLf * SampleRateFine + texParams.xy, lvlParams.w);
    float3 uvc = float3(positionLc * SampleRateCoarse + texParams.zw, lvlParams.w + 1);

    // blend elevation value
    // lvlParams.w : level
    float h = SampleClipmapLevel(Height, PointWrap, uvf, uvc, alpha.x);
    h *= HeightMapScale;

    const float2 pl = lerp(positionLf, positionLc, alpha.x);
    uvf = float3(pl * SampleRateFine + texParams.xy, lvlParams.w);
    uvc = float3(pl * SampleRateCoarse + texParams.zw, lvlParams.w + 1);

    positionW = float3(xz.x, h, xz.y);
    positionH = float4(positionW, 1);
    positionH = mul(positionH, ViewProjection);
    color = LoadColor(lvlParams.z); // lvlParams.z : color
    texCoordF = uvf;
    texCoordC = float4(uvc, alpha.x);
}
