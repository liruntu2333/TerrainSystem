#include "Grass.hlsli"
static const int vcLod0 = 15;
static const int vcLod1 = 7;

static const float2 lod0[vcLod0] =
{
    float2(-1.0, 0.0),
    float2(+1.0, 0.0),
    float2(-1.0, 1.0 / 7.0),
    float2(+1.0, 1.0 / 7.0),
    float2(-1.0, 2.0 / 7.0),
    float2(+1.0, 2.0 / 7.0),
    float2(-1.0, 3.0 / 7.0),
    float2(+1.0, 3.0 / 7.0),
    float2(-1.0, 4.0 / 7.0),
    float2(+1.0, 4.0 / 7.0),
    float2(-1.0, 5.0 / 7.0),
    float2(+1.0, 5.0 / 7.0),
    float2(-1.0, 6.0 / 7.0),
    float2(+1.0, 6.0 / 7.0),
    float2(0.0, 1.0)
};

static const float2 lod0Fold[vcLod0] =
{
    float2(0.0, 1.0),
    float2(+1.0, 3.0 / 4.0),
    float2(-1.0, 2.0 / 4.0),
    float2(+1.0, 2.0 / 4.0),
    float2(-1.0, 1.0 / 4.0),
    float2(+1.0, 1.0 / 4.0),
    float2(-1.0, 0.0),
    float2(+1.0, 0.0),

    float2(-1.0, 1.0 / 4.0),
    float2(+1.0, 1.0 / 4.0),
    float2(-1.0, 2.0 / 4.0),
    float2(+1.0, 2.0 / 4.0),
    float2(-1.0, 3.0 / 4.0),
    float2(+1.0, 3.0 / 4.0),
    float2(0.0, 1.0)
};

static const float2 lod1[vcLod1] =
{
    float2(-1.0, 0.0),
    float2(+1.0, 0.0),
    float2(-1.0, 1.0 / 3.0),
    float2(+1.0, 1.0 / 3.0),
    float2(-1.0, 2.0 / 3.0),
    float2(+1.0, 2.0 / 3.0),
    float2(0.0, 1.0)
};

static const float2 lod1Fold[vcLod1] =
{
    float2(0.0, 1.0),
    float2(+1.0, 1.0 / 2.0),
    float2(-1.0, 0.0),
    float2(+1.0, 0.0),
    float2(-1.0, 1.0 / 2.0),
    float2(+1.0, 1.0 / 2.0),
    float2(0.0, 1.0)
};

static const int morphTarget[vcLod0] =
{
    0, 1, 0, 1, 0, 1, 2, 3, 4, 5, 6, 6, 6, 6, 6
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
    float3 posV1            = inst.posV1;
    float3 posV2            = inst.posV2;
    const float3 bladeDir   = inst.bladeDir;

    float2 uv;

    const bool isLod0 = inst.lod < 1.0;
    bool backSide     = false;

    //if (length(posV1) + length(posV2) < foldHeight)
    //{
    //    const float3 up         = normalize(posV1);
    //    const float3 curveFront = normalize(cross(up, bladeDir));

    //    int vertCnt = isLod0 ? vcLod0 : vcLod1;
    //    backSide    = vertexId <= vertCnt / 2;
    //    float side  = backSide ? -2.0 : 0.0;
    //    posV1 += side * dot(posV1, curveFront) * curveFront;
    //    posV2 += side * dot(posV2, curveFront) * curveFront;
    //    if (isLod0)
    //    {
    //        //uv = lerp(lod0[vertexId], lod1[morphTarget[vertexId]], inst.lod);
    //        uv = lerp(lod0Fold[vertexId], lod1Fold[morphTarget[vertexId]], inst.lod);
    //    }
    //    else
    //    {
    //        uv = lod1Fold[vertexId];
    //    }
    //}
    //else 
    if (isLod0)
    {
        uv = lerp(lod0[vertexId], lod1[morphTarget[vertexId]], inst.lod);
    }
    else
    {
        uv = lod1[vertexId];
    }

    float3 pos = QuadBezierP0Zero(posV1, posV2, uv.y);

    //const float3 dir  = normalize(lerp(inst.bladeDir, bladeDir, uv.y));
    const float width = inst.halfWidth * (1.0 - uv.y);

    pos += bladeDir * width * uv.x;
    pos += v0;

    const float3 dCurve = QuadBezierDerivativeP0Zero(posV1, posV2, uv.y);
    float3 wNor         = normalize(cross(dCurve, bladeDir));
    if (backSide)
    {
        wNor = -wNor;
    }
    wNor = normalize(wNor + uv.x * bladeDir * 0.5);
    // add a little curvature to normal
    uv.x = uv.x * 0.5 + 0.5;

    VertexOut vout;
    vout.Position = mul(float4(pos, 1.0), viewProj);
    vout.Normal   = wNor;
    vout.Texture  = uv;
    vout.Lod      = inst.lod;
    vout.WldPos   = pos;
    return vout;
}
