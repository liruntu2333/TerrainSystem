struct Vertex
{
    float3 Position;
    float3 Normal;
    float2 Texture;
};

cbuffer ModelConstants : register(b0)
{
    float4x4 worldViewProjection;
    float4x4 viewProjection;
    float4x4 world;
    float3 camPos;
    float pad;
}

StructuredBuffer<Vertex> vertices : register(t0);
StructuredBuffer<uint> indices : register(t1);

struct VertexOut
{
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL;
    float2 Texture : TEXCOORD0;
    float3 WldPos : TEXCOORD1;
};

VertexOut main(uint id : SV_VertexID)
{
    VertexOut vout;
    Vertex v      = vertices[indices[id]];
    vout.Position = mul(float4(v.Position, 1.0), worldViewProjection);
    vout.Normal   = mul(float4(v.Normal, 0.0), world).xyz;
    vout.Texture  = v.Texture;
    vout.WldPos   = mul(float4(v.Position, 1.0), world).xyz;
    return vout;
}
