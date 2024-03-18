#ifndef _PLANET_HLSLI_
#define _PLANET_HLSLI_

#define DEEP_OCEAN_COLOR float3(0.0, 0.2, 1.0)
#define SHALLOW_OCEAN_COLOR float3(0.5, 0.5, 0.5)
#define OCEAN_ALPHA 0.5

struct VertexOut
{
    float4 Position : SV_Position;
    float4 NormalDist : TEXCOORD0;
};

cbuffer cb0 : register(b0)
{
    float3 faceUp;
    float gridSize;
    float3 faceRight;
    float gridOffsetU;
    float3 faceBottom;
    float gridOffsetV;

    float4x4 worldViewProj;
    float4x4 world;
    float4x4 worldInvTrans;

    float4 featureNoiseSeed;
    float4 sharpnessNoiseSeed;
    float4 slopeErosionNoiseSeed;
    float4 perturbNoiseSeed;

    float4x4 featureNoiseRotation;
    float4x4 sharpnessNoiseRotation;
    float4x4 slopeErosionNoiseRotation;
    float4x4 perturbNoiseRotation;

    int geometryOctaves;
    float lacunarity;
    float gain;
    float radius;

    float elevation;
    float altitudeErosion;
    float ridgeErosion;
    float baseFrequency;

    float baseAmplitude;
    float3 pad;

    float2 sharpness;
    float sharpnessBaseFrequency;
    float sharpnessLacunarity;

    float2 slopeErosion;
    float slopeErosionBaseFrequency;
    float slopeErosionLacunarity;

    float2 perturb;
    float perturbBaseFrequency;
    float perturbLacunarity;

    float3 camPos;
    float oceanLevel;

    float4 debugCol;
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
    return mod289((x * 34.0 + 10.0) * x);
}


float4 taylorInvSqrt(float4 r)
{
    return (float4)1.79284291400159 - r * 0.85373472095314;
}

float3 taylorInvSqrt(float3 r)
{
    return 1.79284291400159 - 0.85373472095314 * r;
}

// uint3 pcg3d(uint3 v)
// {
//     v = v * 1664525u + 1013904223u;
//     v.x += v.y * v.z;
//     v.y += v.z * v.x;
//     v.z += v.x * v.y;
//     v ^= v >> 16u;
//     v.x += v.y * v.z;
//     v.y += v.z * v.x;
//     v.z += v.x * v.y;
//     return v;
// }

float4 SimplexGradNoise(float3 v, float4 seed)
{
    const float2 C = float2(1.0 / 6.0, 1.0 / 3.0);
    const float4 D = float4(0.0, 0.5, 1.0, 2.0);

    // First corner
    float3 i  = floor(v + dot(v, C.yyy));
    float3 x0 = v - i + dot(i, C.xxx);

    // Other corners
    float3 g  = step(x0.yzx, x0.xyz);
    float3 l  = 1.0 - g;
    float3 i1 = min(g.xyz, l.zxy);
    float3 i2 = max(g.xyz, l.zxy);

    //   x0 = x0 - 0.0 + 0.0 * C.xxx;
    //   x1 = x0 - i1  + 1.0 * C.xxx;
    //   x2 = x0 - i2  + 2.0 * C.xxx;
    //   x3 = x0 - 1.0 + 3.0 * C.xxx;
    float3 x1 = x0 - i1 + C.xxx;
    float3 x2 = x0 - i2 + C.yyy; // 2.0*C.x = 1/3 = C.y
    float3 x3 = x0 - D.yyy; // -1.0+3.0*C.x = -0.5 = -D.y

    // https://github.com/ashima/webgl-noise/issues/9
    float3 seedInt = floor(float3(seed.xyz) + .5);
    // Permutations
    i        = mod289(i);
    float4 p = permute(permute(permute(
                i.z + float4(0.0, i1.z, i2.z, 1.0) + seedInt.z) +
            i.y + float4(0.0, i1.y, i2.y, 1.0) + seedInt.y) +
        i.x + float4(0.0, i1.x, i2.x, 1.0) + seedInt.x);


    // Gradients: 7x7 points over a square, mapped onto an octahedron.
    // The ring size 17*17 = 289 is close to a multiple of 49 (49*6 = 294)
    float n_  = 0.142857142857; // 1.0/7.0
    float3 ns = n_ * D.wyz - D.xzx;

    float4 j = p - 49.0 * floor(p * ns.z * ns.z); //  mod(p,7*7)

    float4 x_ = floor(j * ns.z);
    float4 y_ = floor(j - 7.0 * x_); // mod(j,N)

    float4 x = x_ * ns.x + ns.yyyy;
    float4 y = y_ * ns.x + ns.yyyy;
    float4 h = 1.0 - abs(x) - abs(y);

    float4 b0 = float4(x.xy, y.xy);
    float4 b1 = float4(x.zw, y.zw);

    //vec4 s0 = vec4(lessThan(b0,0.0))*2.0 - 1.0;
    //vec4 s1 = vec4(lessThan(b1,0.0))*2.0 - 1.0;
    float4 s0 = floor(b0) * 2.0 + 1.0;
    float4 s1 = floor(b1) * 2.0 + 1.0;
    float4 sh = -step(h, float4(0.0, 0.0, 0.0, 0.0));

    float4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
    float4 a1 = b1.xzyw + s1.xzyw * sh.zzww;

    float3 p0 = float3(a0.xy, h.x);
    float3 p1 = float3(a0.zw, h.y);
    float3 p2 = float3(a1.xy, h.z);
    float3 p3 = float3(a1.zw, h.w);

    //Normalise gradients
    float4 norm = taylorInvSqrt(float4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));
    p0 *= norm.x;
    p1 *= norm.y;
    p2 *= norm.z;
    p3 *= norm.w;

    // Mix final noise value
    float4 m     = max(0.5 - float4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
    float4 m2    = m * m;
    float4 m4    = m2 * m2;
    float4 pdotx = float4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3));

    // Determine noise gradient
    float4 temp     = m2 * m * pdotx;
    float3 gradient = -8.0 * (temp.x * x0 + temp.y * x1 + temp.z * x2 + temp.w * x3);
    gradient += m4.x * p0 + m4.y * p1 + m4.z * p2 + m4.w * p3;

    return 105.0 * float4(gradient, dot(m4, pdotx));
}

float SimplexNoise(float3 v, float4 seed)
{
    return SimplexGradNoise(v, seed).w;
}

float SimplexNoise01(float3 v, float4 seed)
{
    return SimplexNoise(v, seed) * 0.5 + 0.5;
}

float4 Billowy(float4 dNoise)
{
    // dNoise *= sign(dNoise.w);

    dNoise.xyz = 2.0 * dNoise.w * dNoise.xyz;
    dNoise.w   = dNoise.w * dNoise.w;
    // dNoise = dNoise * 2.0 + float4(0.0, 0.0, 0.0, -1.0);
    return dNoise;
}

float4 Ridged(float4 dNoise)
{
    // dNoise.xyz *= -sign(dNoise.w);
    // dNoise.w = 1.0 - abs(dNoise.w);

    dNoise.xyz = 2.0 * (dNoise.w + -sign(dNoise.w)) * dNoise.xyz;
    float r    = 1.0 - abs(dNoise.w);
    dNoise.w   = r * r;

    // dNoise.xyz = -2.0 * dNoise.w * dNoise.xyz;
    // dNoise.w   = 1.0 - dNoise.w * dNoise.w;
    // dNoise = dNoise * 2.0 + float4(0.0, 0.0, 0.0, -1.0);
    return dNoise;
}

float4 UberNoiseFbm(float3 unitSphere, int numOctaves = 10)
{
    float noise   = 0.0;
    float3 grad   = 0.0;
    float amp     = baseAmplitude;
    float dampAmp = amp;

    float3 featureSph      = unitSphere,
           sharpnessSph    = unitSphere,
           slopeErosionSph = unitSphere,
           perturbSph      = unitSphere;

    float featureFreq      = baseFrequency,
          sharpnessFreq    = sharpnessBaseFrequency,
          slopeErosionFreq = slopeErosionBaseFrequency,
          perturbFreq      = perturbBaseFrequency;


    float3 slopeErosionGrad = 0.0;
    float3 ridgeErosionGrad = 0.0;

    float currSharpness    = 0.0;
    float currSlopeErosion = 0.0;
    float3 perturbDir      = 0.0;
    perturbDir             = SimplexGradNoise(perturbSph * perturbFreq, perturbNoiseSeed).xyz;

    for (int i = 0; i < numOctaves; i++)
    {
        featureSph      = mul(featureSph, (float3x3)featureNoiseRotation);
        sharpnessSph    = mul(sharpnessSph, (float3x3)sharpnessNoiseRotation);
        slopeErosionSph = mul(slopeErosionSph, (float3x3)slopeErosionNoiseRotation);
        perturbSph      = mul(perturbSph, (float3x3)perturbNoiseRotation);

        currSharpness = lerp(sharpness.x, sharpness.y,
            SimplexNoise01(sharpnessSph * sharpnessFreq, sharpnessNoiseSeed));
        currSlopeErosion = lerp(slopeErosion.x, slopeErosion.y,
            SimplexNoise01(slopeErosionSph * slopeErosionFreq, slopeErosionNoiseSeed));

        sharpnessFreq *= sharpnessLacunarity;
        slopeErosionFreq *= slopeErosionLacunarity;
        perturbFreq *= perturbLacunarity;

		float3 v = featureSph * featureFreq;
        if (amp < 1e-3)
        {
            break;
        }

        float4 gradNoise = SimplexGradNoise(v, featureNoiseSeed);
        float4 billow    = Billowy(gradNoise);
        float4 ridge     = Ridged(gradNoise);

        gradNoise = gradNoise * 0.5 + float4(0.0, 0.0, 0.0, 0.5);
		float3 slopeErosionGradTmp = slopeErosionGrad + gradNoise.xyz * currSlopeErosion;
        gradNoise = lerp(gradNoise, ridge, smoothstep(0.0, 1.0, max(0.0, currSharpness)));
        gradNoise = lerp(gradNoise, billow, smoothstep(0.0, 1.0, abs(min(0.0, currSharpness))));

		float erosion = 1.0 / (1.0 + dot(slopeErosionGrad, slopeErosionGrad));

        noise += dampAmp * erosion * gradNoise.w;
        grad += dampAmp * erosion * gradNoise.xyz;
        featureFreq *= lacunarity;
        amp *= gain;
        // amp *= lerp(1.0, smoothstep(0.0, 1.0, sum), altitudeErosion);
        dampAmp = amp;
		slopeErosionGrad = slopeErosionGradTmp;
	}

    return float4(grad, noise);
}

#endif
