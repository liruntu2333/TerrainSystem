
void main(
    in float4 positionH : SV_POSITION,
    in float4 colorIn : COLOR,
    in float3 texCoord : TEXCOORD0,
	in float h : TEXCOORD1,

    out float4 colorOut : SV_TARGET)
{
	colorOut = colorIn;
}
