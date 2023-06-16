void main(
    in float4 positionL : SV_POSITION,
    in float4 color : COLOR0,
    out float4 colorOut : SV_TARGET
    )
{
    colorOut = color;
}
