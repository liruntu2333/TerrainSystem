#include "Grass.hlsli"
static const float2 highLod[15] =
{
    float2(-1.0, 0.0),
    float2(+1.0, 0.0),
    float2(-1.0, 0.14285714),
    float2(+1.0, 0.14285714),
    float2(-1.0, 0.28571428),
    float2(+1.0, 0.28571428),
    float2(-1.0, 0.42857142),
    float2(+1.0, 0.42857142),
    float2(-1.0, 0.57142857),
    float2(+1.0, 0.57142857),
    float2(-1.0, 0.71428571),
    float2(+1.0, 0.71428571),
    float2(-1.0, 0.85714285),
    float2(+1.0, 0.85714285),
    float2(-1.0, 1.0)
};

static const float2 lowLod[7] =
{
    float2(-1.0, 0.0),
    float2(+1.0, 0.0),
    float2(-1.0, 0.33333333),
    float2(+1.0, 0.33333333),
    float2(-1.0, 0.66666667),
    float2(+1.0, 0.66666667),
    float2(-1.0, 1.0)
};

float3 QuadBezierP0Zero(const float3 p1, const float3 p2, const float t)
{
    return 2.0 * (1.0 - t) * t * p1 + t * t * p2;
}

float3 QuadBezierDerivativeP0Zero(const float3 p1, const float3 p2, const float t)
{
    return 2.0 * (1.0 - t) * p1 + 2.0 * t * (p2 - p1);
}

StructuredBuffer<InstanceData> instanceData : register(t0);

VertexOut main(uint vertexId : SV_VertexID, const uint instanceId : SV_InstanceID)
{
    const InstanceData inst = instanceData[instanceId];
    const float3 v0         = inst.pos;
    //const float3 v0v1       = inst.posV1;
    //const float3 v0v2       = inst.posV2;
    //const float3 bladeDir   = normalize(cross(v0v1, v0v2));
    float2 uv = highLod[vertexId];

    float3 pos = QuadBezierP0Zero(inst.posV1, inst.posV2, uv.y);

    //const float3 dir  = normalize(lerp(inst.bladeDir, bladeDir, uv.y));
    const float width = inst.maxWidth * (1.0 - uv.y);
    pos += inst.bladeDir * width * uv.x;
    pos += v0;


    const float3 dCurve = QuadBezierDerivativeP0Zero(inst.posV1, inst.posV2, uv.y);
    float3 wNor         = normalize(cross(dCurve, inst.bladeDir));
    wNor                = normalize(wNor + uv.x * inst.bladeDir * 0.25);
    // add a little curvature to normal
    uv.x = uv.x * 0.5 + 0.5;

    VertexOut vout;
    vout.Position = mul(float4(pos, 1.0), viewProj);
    vout.Normal   = wNor;
    vout.Texture  = uv;
    return vout;
}
