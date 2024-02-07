cbuffer Uniforms : register(b0)
{
    float4x4 viewProj;
    float4x4 baseWorld;
    float4x4 vpForCull;
    uint numBaseTriangle;
    float density;
    float2 heightRange;
    float2 widthRange;
    float2 bendRange;
    float4 gravity;
    float4 wind;
    float windWave;
    float2 nearFar;
    int distCulling;
    int orientCulling;
    int frustumCulling;
    int occlusionCulling;
    int debug;
    float3 camPos;
    float orientThreshold;
    //float4 planes[6];
    float lod0Dist;
    int pad[3];
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
    float3 bladeDir;
    float maxWidth;
    float lod;
};

struct VertexOut
{
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL;
    float2 Texture : TEXCOORD0;
    float Lod : TEXCOORD1;
    float3 WldPos : TEXCOORD2;
};
