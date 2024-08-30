cbuffer cbPerObject : register(b0)
{
	float4x4 WorldViewProj;
	float ElevationRatio;
	float3 BaseColor;
	float InvMaxError;
	float3 Pad;
};

struct Output
{
    float4 Position : SV_POSITION;
    float2 ElevationLocal : TEXCOORD0;
	float3 Normal : NORMAL;
};

Texture2D<float> OriginElevation : register(t0);
Texture2D<float> CompressedElevation : register(t1);

float3 ComputeNormal(int x, int y)
{
	uint w, h;
	CompressedElevation.GetDimensions(w, h);

	float eLeft = CompressedElevation.Load(int3(max(x - 1, 0), y, 0));
	float eRight = CompressedElevation.Load(int3(min(x + 1, w - 1), y, 0));
	float eTop = CompressedElevation.Load(int3(x, max(y - 1, 0), 0));
	float eBottom = CompressedElevation.Load(int3(x, min(y + 1, h - 1), 0));
    float eTopLeft = CompressedElevation.Load(int3(max(x - 1, 0), max(y - 1, 0), 0));
    float eTopRight = CompressedElevation.Load(int3(min(x + 1, w - 1), max(y - 1, 0), 0));
    float eBottomLeft = CompressedElevation.Load(int3(max(x - 1, 0), min(y + 1, h - 1), 0));
    float eBottomRight = CompressedElevation.Load(int3(min(x + 1, w - 1), min(y + 1, h - 1), 0));

    float3 dx = float3(4, (eBottomRight - eBottomLeft + eRight - eLeft + eTopRight - eTopLeft) * ElevationRatio / 3.0f, 0);
    float3 dz = float3(0, (eBottomLeft - eTopLeft + eBottom - eTop + eBottomRight - eTopRight) * ElevationRatio / 3.0f, 4);

    float3 normal = normalize(cross(dz, dx));

	return normal;
}

Output main(uint id : SV_VertexID)
{
    uint w, h;
    OriginElevation.GetDimensions(w, h);
    uint x     = id % w;
    uint y     = id / w;
    float3 pos = float3(float(x) - float(w >> 1), 0, y - float(h >> 1));
    float e0   = OriginElevation.Load(int3(x, y, 0));
    float e1   = CompressedElevation.Load(int3(x, y, 0));
    pos.y      = e1 * ElevationRatio;
	pos.xz *= 2.0;

	float3 nor = ComputeNormal(x, y);

    Output output;
    output.Position       = mul(float4(pos, 1), WorldViewProj);
	output.ElevationLocal = float2(e0, e1);
    output.Normal = nor;
    return output;
}
