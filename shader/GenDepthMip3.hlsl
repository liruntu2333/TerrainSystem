Texture2D<float> depthSource : register(t0);
RWTexture2D<float> depthMip0 : register(u0);
RWTexture2D<float> depthMip1 : register(u1);
RWTexture2D<float> depthMip2 : register(u2);
RWTexture2D<float> depthMip3 : register(u3);

groupshared float depthMax[8][8];

[numthreads(8, 8, 1)]
void main(uint3 threadId : SV_DispatchThreadID, uint3 groupThreadId : SV_GroupThreadId)
{
    float d0   = depthSource.Load(int3(threadId.x * 2, threadId.y * 2, 0));
    float d1   = depthSource.Load(int3(threadId.x * 2 + 1, threadId.y * 2, 0));
    float d2   = depthSource.Load(int3(threadId.x * 2, threadId.y * 2 + 1, 0));
    float d3   = depthSource.Load(int3(threadId.x * 2 + 1, threadId.y * 2 + 1, 0));
    float dMax = max(max(max(d0, d1), d2), d3);

    depthMip0[int2(threadId.x * 2, threadId.y * 2)]         = d0;
    depthMip0[int2(threadId.x * 2 + 1, threadId.y * 2)]     = d1;
    depthMip0[int2(threadId.x * 2, threadId.y * 2 + 1)]     = d2;
    depthMip0[int2(threadId.x * 2 + 1, threadId.y * 2 + 1)] = d3;

    depthMip1[threadId.xy]                     = dMax;
    depthMax[groupThreadId.x][groupThreadId.y] = dMax;
    GroupMemoryBarrierWithGroupSync();

    if (groupThreadId.x % 2 == 0 && groupThreadId.y % 2 == 0)
    {
        groupThreadId /= 2;
        d0   = dMax;
        d1   = depthMax[groupThreadId.x + 1][groupThreadId.y];
        d2   = depthMax[groupThreadId.x][groupThreadId.y + 1];
        d3   = depthMax[groupThreadId.x + 1][groupThreadId.y + 1];
        dMax = max(max(max(d0, d1), d2), d3);

        depthMip2[threadId.xy / 2] = dMax;
    }

    GroupMemoryBarrierWithGroupSync();

    if (groupThreadId.x % 2 == 0 && groupThreadId.y % 2 == 0)
    {
        groupThreadId /= 2;
        d0   = dMax;
        d1   = depthMax[groupThreadId.x + 1][groupThreadId.y];
        d2   = depthMax[groupThreadId.x][groupThreadId.y + 1];
        d3   = depthMax[groupThreadId.x + 1][groupThreadId.y + 1];
        dMax = max(max(max(d0, d1), d2), d3);

        depthMip3[threadId.xy / 4] = dMax;
    }
}
