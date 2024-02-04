#include "ShaderUtil.hlsli"

struct VertexOut
{
	float4 Position : SV_POSITION;
	float3 Normal : NORMAL;
	float2 Texture : TEXCOORD0;
};

Texture2D albedo : register(t0);
SamplerState anisotropicClamp : register(s0);

float4 main(VertexOut pin) : SV_TARGET
{
	float3 normal = normalize(pin.Normal);
	float3 col = Shade(normal, float3(0.0, 1.0, 0.0), 0.1, 1.0) * albedo.Sample(anisotropicClamp, pin.Texture);
	col = ToneMapping(col);
	return float4(col, 1.0f);
}
