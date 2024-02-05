cbuffer Uniforms : register(b0)
{
    float4x4 viewProj;
    float4x4 baseWorld;
    uint numBaseTriangle;
    float density;
	float pad[2];
}

struct BaseVertex
{
    float3 position;
    float3 normal;
    float2 textureCoordinate;
};

struct InstanceData
{
    float3 pos;
    float3 posV1;
    float3 posV2;
    float maxWidth;
    uint hash;
};

struct VertexOut
{
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL;
    float2 Texture : TEXCOORD0;
};
