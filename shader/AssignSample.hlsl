#include "Grass.hlsli"

StructuredBuffer<BaseVertex> vertices : register(t0);
StructuredBuffer<uint> indices : register(t1);

RWBuffer<uint> dispatchArg : register(u0);
AppendStructuredBuffer<uint> triangleIndex : register(u1);

[numthreads(256, 1, 1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
    if (threadId.x == 0u)
    {
		dispatchArg[0] = numBaseTriangle;
        dispatchArg[1] = 1u;
        dispatchArg[2] = 1u;
    }
 //   DeviceMemoryBarrier();

 //   if (threadId.x >= numBaseTriangle) return;

 //   const uint ti = threadId.x;
 //   const BaseVertex baseV0 = vertices[indices[ti * 3 + 0]];
 //   const BaseVertex baseV1 = vertices[indices[ti * 3 + 1]];
	//const BaseVertex baseV2 = vertices[indices[ti * 3 + 2]];

 //   const float3 cross12 = cross(baseV1.position - baseV0.position, baseV2.position - baseV0.position);
 //   const float area     = length(cross12) * 0.5;

	//uint numInstances = round(density * area);
 //   numInstances = max(numInstances, 1u);
	////round(density * area);
 //   //numInstances = 1;
	
	//for (uint i = 0; i < numInstances; ++i)
 //   {
	//	InterlockedAdd(dispatchArg[0], 1u);
	//	triangleIndex.Append(ti << 10 | i);
	//}
}
