#include "Grass.hlsli"

#define Pi 3.14159265359
#define TwoPi 6.28318530718
#define PiDivTwo 1.57079632679

// Hash function from H. Schechter & R. Bridson, goo.gl/RXiKaH
uint Hash(uint s)
{
    s ^= 2747636419u;
    s *= 2654435769u;
    s ^= s >> 16;
    s *= 2654435769u;
    s ^= s >> 16;
    s *= 2654435769u;
    return s;
}

float RandF(inout uint seed)
{
    seed = Hash(seed);
    return float(seed) / 4294967295.0; // 2^32-1
}

float3 CalculateGroundPosV1(in float3 groundPosV2, in float3 bladeUp, in float height, in float invHeight)
{
    const float3 g      = groundPosV2 - dot(groundPosV2, bladeUp) * bladeUp;
    const float v2ratio = abs(length(g) * invHeight);
    const float fac     = max(1.0f - v2ratio, 0.05f * max(v2ratio, 1.0f));
    return bladeUp * height * fac;
}

void MakePersistentLength(inout float3 groundPosV1, inout float3 groundPosV2, in float height)
{
    //Persistent length
    float3 v01       = groundPosV1;
    float3 v12       = groundPosV2 - groundPosV1;
    const float lv01 = length(v01);
    const float lv12 = length(v12);

    const float L1 = lv01 + lv12;
    const float L0 = length(groundPosV2);
    const float L  = (2.0f * L0 + L1) / 3.0f; //http://steve.hollasch.net/cgindex/curves/cbezarclen.html

    const float ldiff = height / L;
    v01               = v01 * ldiff;
    v12               = v12 * ldiff;
    groundPosV1       = v01;
    groundPosV2       = groundPosV1 + v12;
}

void EnsureValidV2Pos(inout float3 groundPosV2, in float3 bladeUp)
{
    groundPosV2 += bladeUp * -min(dot(bladeUp, groundPosV2), 0.0f);
}

RWBuffer<uint> drawArg : register(u0);
AppendStructuredBuffer<InstanceData> lod0Inst : register(u1);
AppendStructuredBuffer<InstanceData> lod1Inst : register(u2);

StructuredBuffer<BaseVertex> vertices : register(t0);
StructuredBuffer<uint> indices : register(t1);
Texture2D<float> depth : register(t2);
//StructuredBuffer<uint> triangleIndex : register(t2);

[numthreads(256, 1, 1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
    if (threadId.x == 0u)
    {
        //struct D3D11_DRAW_INDEXED_INSTANCED_INDIRECT_ARGS
        //{
        //    UINT IndexCountPerInstance;
        //    UINT InstanceCount;
        //    UINT StartIndexLocation;
        //    INT  BaseVertexLocation;
        //    UINT StartInstanceLocation;
        //};
        // LOD 0
        drawArg[0] = 15u;
        drawArg[1] = 0u;
        drawArg[2] = 0u;
        drawArg[3] = 0;
        drawArg[4] = 0u;
        // LOD 1
        drawArg[5] = 7u;
        drawArg[6] = 0u;
        drawArg[7] = 0u;
        drawArg[8] = 0;
        drawArg[9] = 0u;
    }

    DeviceMemoryBarrier();

    if (threadId.x >= numBaseTriangle)
    {
        return;
    }

    //uint seed               = triangleIndex[threadId.x];
    const uint ti           = threadId.x;
    uint seed               = ti;
    const BaseVertex baseV0 = vertices[indices[ti * 3 + 0]];
    const BaseVertex baseV1 = vertices[indices[ti * 3 + 1]];
    const BaseVertex baseV2 = vertices[indices[ti * 3 + 2]];

    const float3 cross12 = cross(baseV1.position - baseV0.position, baseV2.position - baseV0.position);
    const float area     = length(cross12) * 0.5;

    //const float3 faceUp = normalize(mul(cross12, invTansBaseWorld));
    const float3 faceUp = normalize(cross12);
    //const float3 up   = float3(0.0, 1.0, 0.0);
    //const float c     = dot(up, n);
    //float3 k          = cross(up, n);
    //const float s     = length(k);
    //const float3x3 vx = float3x3(
    //    float3(0.0, k.z, -k.y),
    //    float3(-k.z, 0.0, k.x),
    //    float3(k.y, -k.x, 0.0));
    //// Rodrigues' rotation formula
    //// https://math.stackexchange.com/questions/180418/calculate-rotation-matrix-to-align-vector-a-to-vector-b-in-3d
    //float3x3 r3 = float3x3(
    //    float3(1, 0, 0),
    //    float3(0, 1, 0),
    //    float3(0, 0, 1)) + vx * s + mul(vx, vx) * (1 - c);
    //const float4x4 R = float4x4(
    //    float4(r3[0], 0),
    //    float4(r3[1], 0),
    //    float4(r3[2], 0),
    //    float4(0, 0, 0, 1));

    float numBlade          = density * area;
    float numRange          = 0.5 / numBlade;
    const uint numInstances = round(numBlade * lerp(1.0 - numRange, 1.0 + numRange, RandF(seed)));
    for (uint i = 0; i < numInstances; ++i)
    {
        // https://chrischoy.github.io/research/barycentric-coordinate-for-mesh-sampling/
        const float r1     = RandF(seed);
        const float r2     = RandF(seed);
        const float sqrtR1 = sqrt(r1);
        const float alpha  = 1.0 - sqrtR1;
        const float beta   = (1.0 - r2) * sqrtR1;
        const float gamma  = r2 * sqrtR1;

        float3 groundPos = baseV0.position * alpha + baseV1.position * beta + baseV2.position * gamma;
        groundPos        = mul(float4(groundPos, 1.0), baseWorld).xyz;

        float3 bladeUp = normalize(baseV0.normal * alpha + baseV1.normal * beta + baseV2.normal * gamma);

        const float dirPhi = lerp(0.0, TwoPi, RandF(seed));
        float sd, cd;
        sincos(dirPhi, sd, cd);
        float3 tmp        = normalize(float3(sd, sd + cd, cd)); //arbitrary vector for finding normal vector
        float3 bladeDir   = normalize(cross(bladeUp, tmp));
        float3 bladeFront = normalize(cross(bladeUp, bladeDir));

        bladeUp    = normalize(mul(float4(bladeUp, 0.0), baseWorld).xyz);
        bladeDir   = normalize(mul(float4(bladeDir, 0.0), baseWorld).xyz);
        bladeFront = normalize(mul(float4(bladeFront, 0.0), baseWorld).xyz);

        const float height    = lerp(heightRange.x, heightRange.y, RandF(seed));
        const float invHeight = 1.0 / height;

        //float3 camDirProj       = camDir - dot(camDir, bladeUp) * bladeUp;
        const float3 camDir     = camPos - groundPos;
        const float camDist     = length(camDir);
        const float3 camDirNorm = camDir / camDist;

        //if (frustumCulling)
        //{
        //	const float boostHeight = height;
        //	if (dot(float4(groundPos, 1.0), planes[0]) > boostHeight ||
        //              dot(float4(groundPos, 1.0), planes[1]) > boostHeight ||
        //              dot(float4(groundPos, 1.0), planes[2]) > boostHeight ||
        //              dot(float4(groundPos, 1.0), planes[3]) > boostHeight ||
        //              dot(float4(groundPos, 1.0), planes[4]) > boostHeight ||
        //              dot(float4(groundPos, 1.0), planes[5]) > boostHeight)
        //	{
        //		continue;
        //	}
        //}

        const float width = lerp(widthRange.x, widthRange.y, RandF(seed));
        //const float width = 10.0;
        //const float dirTheta = lerp(0.2, 0.8, RandF(seed)) * PiDivTwo;
        //float ts, tc;
        //sincos(dirTheta, ts, tc);
        const float bendingFac = lerp(bendRange.x, bendRange.y, RandF(seed));
        float3 groundPosV2     = bladeUp * height;

        //gravity
        //float3 grav     = normalize(gravityVec.xyz) * gravityVec.w * (1.0f - useGravityPoint) + normalize(gravityPoint.xyz - v2) * gravityPoint.w * useGravityPoint; //towards mass center
        float3 grav = gravity.xyz * gravity.w;
        float sign  = step(-0.01f, dot(gravity.xyz, bladeFront)) * 2.0f - 1.0f;
        //grav += sign * bladeFront * h * (gravityVec.w * (1.0f - useGravityPoint) + gravityPoint.w * useGravityPoint) * 0.25f; //also a bit forward !!CARE!! 0.25f is some random constant
        grav += sign * bladeFront * height * gravity.w * 0.25f; //also a bit forward !!CARE!! 0.25f is some random constant
        grav = grav * height * bendingFac; //apply bending fac and dt

        //wind
        float windageHeight = abs(dot(groundPosV2, bladeUp)) * invHeight;
        float windageDir    = 1.0 - abs(dot(wind.xyz, normalize(groundPosV2)));
        float windPos       = 1.0 - max((
            cos((groundPos.x + groundPos.z) * 0.25 + windWave) +
            sin((groundPos.x + groundPos.y) * 0.15 + windWave) +
            sin((groundPos.y + groundPos.z) * 0.2 + windWave)) / 3.0, 0.0);
        float3 w = wind.xyz * wind.w * windageDir * windageHeight * windPos * windPos * bendingFac; //!!CARE!! windPos^2 is just random

        // add a little bit of ambient wave to the grass
        w += sin(groundPos.x * 0.3 + groundPos.y * 0.5 + groundPos.z * 0.4) * bladeFront * 0.2 * bendingFac;

        groundPosV2 += grav + w;

        EnsureValidV2Pos(groundPosV2, bladeUp);

        float3 groundPosV1 = CalculateGroundPosV1(groundPosV2, bladeUp, height, invHeight);
        //Grass length correction
        MakePersistentLength(groundPosV1, groundPosV2, height);

        // vec3 midPoint = 0.25f * pos + 0.5f * wV1 + 0.25f * wV2;
        float3 wMidPoint = 0.5f * groundPosV1 + 0.25f * groundPosV2;
        wMidPoint += groundPos;
        float3 wV1       = groundPosV1 + groundPos;
        float3 wV2       = groundPosV2 + groundPos;
        //float camDistMid = length(wMidPoint - camPos);
        //float camDistV2  = length(wV2 - camPos);

        //View Frustum Culling
        float4 pNDC   = mul(float4(groundPos, 1.0f), vpForCull);
        float4 midNDC = mul(float4(wMidPoint, 1.0f), vpForCull);
        float4 v2NDC  = mul(float4(wV2, 1.0f), vpForCull);


        if (orientCulling && abs(dot(camDirNorm, bladeDir)) >= orientThreshold)
        {
            continue;
        }

        if (distCulling)
        {
			if (seed % 500 >= uint(ceil(max(1.0 - (camDist - 0.0) * 0.002, 0.0) * 500)))
            {
                continue;
            }
        }

        if (frustumCulling)
        {
            //Add tolerance
            float tolerance  = 0.5f;
            float4 pNDCTol   = pNDC;
            float4 midNDCTol = midNDC;
            float4 v2NDCTol  = v2NDC;

            pNDCTol.w += tolerance;
            midNDCTol.w += tolerance;
            v2NDCTol.w += tolerance;

            if (!(
                pNDCTol.x > -pNDCTol.w && pNDCTol.x < pNDCTol.w &&
                pNDCTol.y > -pNDCTol.w && pNDCTol.y < pNDCTol.w &&
                pNDCTol.z > -pNDCTol.w && pNDCTol.z < pNDCTol.w ||
                midNDCTol.x > -midNDCTol.w && midNDCTol.x < midNDCTol.w &&
                midNDCTol.y > -midNDCTol.w && midNDCTol.y < midNDCTol.w &&
                midNDCTol.z > -midNDCTol.w && midNDCTol.z < midNDCTol.w ||
                v2NDCTol.x > -v2NDCTol.w && v2NDCTol.x < v2NDCTol.w &&
                v2NDCTol.y > -v2NDCTol.w && v2NDCTol.y < v2NDCTol.w &&
                v2NDCTol.y > -v2NDCTol.w && v2NDCTol.y < v2NDCTol.w
            ))
            {
                continue;
            }
        }

        //Depth buffer culling
        if (occlusionCulling)
        {
            uint2 rtWh;
            depth.GetDimensions(rtWh.x, rtWh.y);
            pNDC.xyz /= pNDC.w;
            midNDC.xyz /= midNDC.w;
            v2NDC.xyz /= v2NDC.w;
            float2 uvP   = pNDC.xy * float2(0.5, -0.5) + 0.5;
            float2 uvMid = midNDC.xy * float2(0.5, -0.5) + 0.5;
            float2 uvV2  = v2NDC.xy * float2(0.5, -0.5) + 0.5;
            //texelFetch(depthTexture, ivec2(uvP * widthHeight), 1).x;
            const float d0 = depth.Load(int3(uvP * rtWh, 0));
            //texelFetch(depthTexture, ivec2(uvMid * widthHeight), 2).x;
            const float dm = depth.Load(int3(uvMid * rtWh, 0));
            //texelFetch(depthTexture, ivec2(uvV2 * widthHeight), 3).x;
            const float d2 = depth.Load(int3(uvV2 * rtWh, 0));
            if (d0 < pNDC.z && dm < midNDC.z && d2 < v2NDC.z)
            {
                continue;
            }
        }

        //const float4x4 S = float4x4(
        //    float4(scl, 0, 0, 0),
        //    float4(0, scl, 0, 0),
        //    float4(0, 0, scl, 0),
        //    float4(0, 0, 0, 1));

        //const float4x4 T = float4x4(
        //    float4(1, 0, 0, 0),
        //    float4(0, 1, 0, 0),
        //    float4(0, 0, 1, 0),
        //    float4(pos, 1));

        //float4x4 world = S;
        //world          = mul(world, R);
        //world          = mul(world, T);
        //world          = mul(world, baseWorld);
        InstanceData instance;
        instance.pos      = groundPos;
        instance.posV1    = groundPosV1;
        instance.posV2    = groundPosV2;
        instance.bladeDir = bladeDir;
        instance.maxWidth = width;
        if (camDist < lod0Dist)
        {
            const float transition = max((camDist - 0.8 * lod0Dist) / (0.2 * lod0Dist), 0.0);
            instance.lod           = transition;
            lod0Inst.Append(instance);
            InterlockedAdd(drawArg[1], 1u);
        }
        else
        {
            instance.lod = 1.0;
            lod1Inst.Append(instance);
            InterlockedAdd(drawArg[6], 1u);
        }
    }
}
