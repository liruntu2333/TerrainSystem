#ifndef _PLANET_HLSLI_
#define _PLANET_HLSLI_

struct VertexOut
{
    float4 Position : SV_Position;
    float4 NormalDist : TEXCOORD0;
};

cbuffer cb0 : register(b0)
{
    float4x4 worldViewProj;
    float4x4 world;

    int geometryOctaves;
    int normalOctaves;
    float lacunarity;
    float gain;

    float radius;
    float elevation;
    int interpolateNormal;
	float sharpness;

    float3 camPos;
	float baseAmplitude;

    float4 debugCol;
}

float4 mod(float4 x, float4 y)
{
    return x - y * floor(x / y);
}

float3 mod(float3 x, float3 y)
{
    return x - y * floor(x / y);
}

float2 mod289(float2 x)
{
    return x - floor(x / 289.0) * 289.0;
}

float3 mod289(float3 x)
{
    return x - floor(x / 289.0) * 289.0;
}

float4 mod289(float4 x)
{
    return x - floor(x / 289.0) * 289.0;
}

float4 permute(float4 x)
{
    return mod289(((x * 34.0) + 1.0) * x);
}

float3 permute(float3 x)
{
    return mod289((x * 34.0 + 1.0) * x);
}

float4 taylorInvSqrt(float4 r)
{
    return (float4)1.79284291400159 - r * 0.85373472095314;
}

float3 taylorInvSqrt(float3 r)
{
    return 1.79284291400159 - 0.85373472095314 * r;
}

// float3 fade(float3 t)
// {
//     return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
// }
//
// float2 fade(float2 t)
// {
//     return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
// }
//
//
// float rand3dTo1d(float3 value, float3 dotDir = float3(12.9898, 78.233, 37.719))
// {
//     //make value smaller to avoid artefacts
//     float3 smallValue = sin(value);
//     //get scalar value from 3d vector
//     float random = dot(smallValue, dotDir);
//     //make value more random by making it bigger and then taking the factional part
//     random = frac(sin(random) * 143758.5453);
//     return random;
// }
//
// float rand2dTo1d(float2 value, float2 dotDir = float2(12.9898, 78.233))
// {
//     float2 smallValue = sin(value);
//     float random      = dot(smallValue, dotDir);
//     random            = frac(sin(random) * 143758.5453);
//     return random;
// }
//
// float rand1dTo1d(float3 value, float mutator = 0.546)
// {
//     float random = frac(sin(value + mutator) * 143758.5453);
//     return random;
// }
//
// //to 2d functions
//
// float2 rand3dTo2d(float3 value)
// {
//     return float2(
//         rand3dTo1d(value, float3(12.989, 78.233, 37.719)),
//         rand3dTo1d(value, float3(39.346, 11.135, 83.155))
//         );
// }
//
// float2 rand2dTo2d(float2 value)
// {
//     return float2(
//         rand2dTo1d(value, float2(12.989, 78.233)),
//         rand2dTo1d(value, float2(39.346, 11.135))
//         );
// }
//
// float2 rand1dTo2d(float value)
// {
//     return float2(
//         rand2dTo1d(value, 3.9812),
//         rand2dTo1d(value, 7.1536)
//         );
// }
//
// //to 3d functions
//
// float3 rand3dTo3d(float3 value)
// {
//     return float3(
//         rand3dTo1d(value, float3(12.989, 78.233, 37.719)),
//         rand3dTo1d(value, float3(39.346, 11.135, 83.155)),
//         rand3dTo1d(value, float3(73.156, 52.235, 09.151))
//         );
// }
//
// float3 rand2dTo3d(float2 value)
// {
//     return float3(
//         rand2dTo1d(value, float2(12.989, 78.233)),
//         rand2dTo1d(value, float2(39.346, 11.135)),
//         rand2dTo1d(value, float2(73.156, 52.235))
//         );
// }
//
// float3 rand1dTo3d(float value)
// {
//     return float3(
//         rand1dTo1d(value, 3.9812),
//         rand1dTo1d(value, 7.1536),
//         rand1dTo1d(value, 5.7241)
//         );
// }

uint3 pcg3d(uint3 v)
{
    v = v * 1664525u + 1013904223u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v ^= v >> 16u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    return v;
}

float4 SimplexNoiseGrad(int seed, float3 v)
{
    const float2 C = float2(1.0 / 6.0, 1.0 / 3.0);

    // First corner
    float3 i  = floor(v + dot(v, C.yyy));
    float3 x0 = v - i + dot(i, C.xxx);

    // Other corners
    float3 g  = step(x0.yzx, x0.xyz);
    float3 l  = 1.0 - g;
    float3 i1 = min(g.xyz, l.zxy);
    float3 i2 = max(g.xyz, l.zxy);

    // x1 = x0 - i1  + 1.0 * C.xxx;
    // x2 = x0 - i2  + 2.0 * C.xxx;
    // x3 = x0 - 1.0 + 3.0 * C.xxx;
    float3 x1 = x0 - i1 + C.xxx;
    float3 x2 = x0 - i2 + C.yyy;
    float3 x3 = x0 - 0.5;

    // Permutations
    i        = mod289(i); // Avoid truncation effects in permutation
    float4 p =
        permute(permute(permute(i.z + float4(0.0, i1.z, i2.z, 1.0))
                + i.y + float4(0.0, i1.y, i2.y, 1.0))
            + i.x + float4(0.0, i1.x, i2.x, 1.0));

    // Gradients: 7x7 points over a square, mapped onto an octahedron.
    // The ring size 17*17 = 289 is close to a multiple of 49 (49*6 = 294)
    float4 j = p - 49.0 * floor(p / 49.0); // mod(p,7*7)

    float4 x_ = floor(j / 7.0);
    float4 y_ = floor(j - 7.0 * x_); // mod(j,N)

    float4 x = (x_ * 2.0 + 0.5) / 7.0 - 1.0;
    float4 y = (y_ * 2.0 + 0.5) / 7.0 - 1.0;

    float4 h = 1.0 - abs(x) - abs(y);

    float4 b0 = float4(x.xy, y.xy);
    float4 b1 = float4(x.zw, y.zw);

    //float4 s0 = float4(lessThan(b0, 0.0)) * 2.0 - 1.0;
    //float4 s1 = float4(lessThan(b1, 0.0)) * 2.0 - 1.0;
    float4 s0 = floor(b0) * 2.0 + 1.0;
    float4 s1 = floor(b1) * 2.0 + 1.0;
    float4 sh = -step(h, 0.0);

    float4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
    float4 a1 = b1.xzyw + s1.xzyw * sh.zzww;

    float3 g0 = float3(a0.xy, h.x);
    float3 g1 = float3(a0.zw, h.y);
    float3 g2 = float3(a1.xy, h.z);
    float3 g3 = float3(a1.zw, h.w);

    // Normalise gradients
    float4 norm = taylorInvSqrt(float4(dot(g0, g0), dot(g1, g1), dot(g2, g2), dot(g3, g3)));
    g0 *= norm.x;
    g1 *= norm.y;
    g2 *= norm.z;
    g3 *= norm.w;

    // Compute noise and gradient at P
    float4 m    = max(0.6 - float4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
    float4 m2   = m * m;
    float4 m3   = m2 * m;
    float4 m4   = m2 * m2;
    float3 grad =
        -6.0 * m3.x * x0 * dot(x0, g0) + m4.x * g0 +
        -6.0 * m3.y * x1 * dot(x1, g1) + m4.y * g1 +
        -6.0 * m3.z * x2 * dot(x2, g2) + m4.z * g2 +
        -6.0 * m3.w * x3 * dot(x3, g3) + m4.w * g3;
    float4 px = float4(dot(x0, g0), dot(x1, g1), dot(x2, g2), dot(x3, g3));
    return 42.0 * float4(grad, dot(m4, px));
}

float SimplexNoise(int seed, float3 v)
{
    return SimplexNoiseGrad(seed, v).w;
}

// float4 BillowyNoiseGrad(int seed, float3 v)
// {
//     float4 dNoise = SimplexNoiseGrad(seed, v);
//     dNoise.xyz    = 2.0 * dNoise.w * dNoise.xyz;
//     dNoise.w      = dNoise.w * dNoise.w;
//     return dNoise;
// }

float4 Billowy(float4 dNoise)
{
    dNoise.xyz = 2.0 * dNoise.w * dNoise.xyz;
    dNoise.w   = dNoise.w * dNoise.w;
    // chi-square distribution of order 1
	return dNoise * 2.0 - 1.0;
}

// float4 RidgedNoiseGrad(int seed, float3 v)
// {
//     float4 dNoise = SimplexNoiseGrad(seed, v);
//     if (dNoise.w > 0.0)
//     {
//         dNoise.xyz = -dNoise.xyz;
//     }
//     dNoise.w = 1.0 - abs(dNoise.w);
//     return dNoise;
// }

float4 Ridged(float4 dNoise)
{
    if (dNoise.w > 0.0)
    {
        dNoise.xyz = -dNoise.xyz;
    }
    dNoise.w = 1.0 - abs(dNoise.w);
	// dNoise.xyz = -2.0 * dNoise.w * dNoise.xyz;
	// dNoise.w = 1.0 - dNoise.w * dNoise.w;
    return dNoise * 2.0 - 1.0;
}

#define FBM_DECLARE

#endif
