#include "ShaderUtil.hlsli"

struct Input
{
	float4 Position : SV_POSITION;
	float2 ElevationLocal : TEXCOORD0;
	float3 Normal : NORMAL;
};

cbuffer cbPerObject : register(b0)
{
    float4x4 WorldViewProj;
    float ElevationRatio;
    float3 BaseColor;
	float InvMaxError;
	float3 Pad;
};

float4 main(Input input) : SV_TARGET
{
	float3 alb = lerp(BaseColor, float3(1, 0, 0), 
		saturate(abs(input.ElevationLocal.x - input.ElevationLocal.y) * ElevationRatio * InvMaxError));
	alb *= EvalSh(normalize(input.Normal));
	// alb *= normalize(input.Normal).y * 0.5;
	alb = GammaCorrect(alb);
    return float4(alb, 0.5);
}
