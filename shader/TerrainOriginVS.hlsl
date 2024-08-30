cbuffer cbPerObject : register(b0)
{
    float4x4 WorldViewProj;
    float ElevationRatio;
};

struct Output
{
    float4 Position : SV_POSITION;
    float2 ElevationLocal : TEXCOORD0;
};

Texture2D<float> OriginElevation : register(t0);

Output main(uint id : SV_VertexID)
{
    uint w, h;
    OriginElevation.GetDimensions(w, h);
    uint x     = id % w;
    uint y     = id / w;
    float3 pos = float3(float(x) - float(w >> 1), 0, y - float(h >> 1));
    float e0   = OriginElevation.Load(int3(x, y, 0));
    pos.y      = e0 * ElevationRatio;
    pos.xz *= 2.0;

    Output output;
    output.Position       = mul(float4(pos, 1), WorldViewProj);
    output.ElevationLocal = float2(e0, e0);
    return output;
}
