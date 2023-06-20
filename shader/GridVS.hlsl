#include "ShaderUtil.hlsli"

SamplerState g_PointWrap : register(s0);
Texture2D<float> g_Height : register(t0);

void main(
    in float2 positionL : SV_POSITION,
    in float4 scales : TEXCOORD0,
    in float4 offsets : TEXCOORD1,
    in float4 colorIn : COLOR0,

    out float4 positionH : SV_POSITION,
    out float4 colorOut : COLOR,
    out float3 texCoord : TEXCOORD,    // coordinates for normal & albedo lookup
    out float3 normal : NORMAL
    )
{
    // convert from grid xy to world xy coordinates
    //  scales.x: grid spacing of current level
    //  scales.zw: origin of current block within world
    int2 groupXy = int2(positionL + scales.zw) % 254;
    if (groupXy.x == 0 && groupXy.y % 2) positionL.y++;
    if (groupXy.y == 0 && groupXy.x % 2) positionL.x++;

    const float2 worldPos = positionL * scales.xx + offsets.xy;

    // compute coordinates for vertex texture
    //  offsets.xy: 1/(w, h) of texture
    //  offsets.zw: origin of block in texture

	const float2 uv = positionL * scales.yy + offsets.zw;
    // compute alpha (transition parameter), and blend elevation.
    float2 alpha = saturate((abs(worldPos - g_ViewPosition) / scales.xx - g_AlphaOffset)
        * g_OneOverWidth);
    alpha.x = max(alpha.x, alpha.y);

	float w = floor(log2(scales.x));
    w += alpha.x;

	float h = g_Height.SampleLevel(g_PointWrap, uv, w);
    h *= g_HeightMapScale;

    positionH = mul(float4(worldPos.x, h, worldPos.y, 1), g_ViewProjection);
    colorOut = colorIn;
    texCoord = float3(uv, w);
}
