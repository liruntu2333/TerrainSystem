
void main(
    in float4 positionL : SV_POSITION,
    in float4 colorIn : COLOR,
    in float3 texCoord : TEXCOORD,
    in float3 normal : NORMAL,

    out float4 colorOut : SV_TARGET)
{
	colorOut = colorIn;
}
