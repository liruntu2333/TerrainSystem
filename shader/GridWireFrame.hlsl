void main(
    in float4 positionH : SV_POSITION,
    in float4 colorIn : COLOR,
    in float3 texCoordF : TEXCOORD0,
    in float4 texCoordC : TEXCOORD1,

    out float4 colorOut : SV_TARGET)
{
    colorOut = colorIn;
}
