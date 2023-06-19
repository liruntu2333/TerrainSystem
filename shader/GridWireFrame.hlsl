
void main(
    in float4 positionL : SV_POSITION,
    in float4 colorIn : COLOR0,
    in float2 texCoord : TEXCOORD0,
    nointerpolation in float level : TEXCOORD1,

    out float4 colorOut : SV_TARGET)
{
	colorOut = colorIn;
}
