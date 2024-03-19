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

// Permutation polynomial: (34x^2 + 6x) mod 289
float3 permute(float3 x)
{
    return mod289((34.0 * x + 10.0) * x);
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

// Modulo 7 without a division
float3 mod7(float3 x)
{
    return x - floor(x * (1.0 / 7.0)) * 7.0;
}

// Cellular noise, returning F1 and F2 in a vec2.
// 3x3x3 search region for good F2 everywhere, but a lot
// slower than the 2x2x2 version.
// The code below is a bit scary even to its author,
// but it has at least half decent performance on a
// modern GPU. In any case, it beats any software
// implementation of Worley noise hands down.

float2 cellular(float3 P)
{
#define K 0.142857142857 // 1/7
#define Ko 0.428571428571 // 1/2-K/2
#define K2 0.020408163265306 // 1/(7*7)
#define Kz 0.166666666667 // 1/6
#define Kzo 0.416666666667 // 1/2-1/6*2
#define jitter 1.0 // smaller jitter gives more regular pattern

    float3 Pi = mod289(floor(P));
    float3 Pf = frac(P) - 0.5;

    float3 Pfx = Pf.x + float3(1.0, 0.0, -1.0);
    float3 Pfy = Pf.y + float3(1.0, 0.0, -1.0);
    float3 Pfz = Pf.z + float3(1.0, 0.0, -1.0);

    float3 p  = permute(Pi.x + float3(-1.0, 0.0, 1.0));
    float3 p1 = permute(p + Pi.y - 1.0);
    float3 p2 = permute(p + Pi.y);
    float3 p3 = permute(p + Pi.y + 1.0);

    float3 p11 = permute(p1 + Pi.z - 1.0);
    float3 p12 = permute(p1 + Pi.z);
    float3 p13 = permute(p1 + Pi.z + 1.0);

    float3 p21 = permute(p2 + Pi.z - 1.0);
    float3 p22 = permute(p2 + Pi.z);
    float3 p23 = permute(p2 + Pi.z + 1.0);

    float3 p31 = permute(p3 + Pi.z - 1.0);
    float3 p32 = permute(p3 + Pi.z);
    float3 p33 = permute(p3 + Pi.z + 1.0);

    float3 ox11 = frac(p11 * K) - Ko;
    float3 oy11 = mod7(floor(p11 * K)) * K - Ko;
    float3 oz11 = floor(p11 * K2) * Kz - Kzo; // p11 < 289 guaranteed

    float3 ox12 = frac(p12 * K) - Ko;
    float3 oy12 = mod7(floor(p12 * K)) * K - Ko;
    float3 oz12 = floor(p12 * K2) * Kz - Kzo;

    float3 ox13 = frac(p13 * K) - Ko;
    float3 oy13 = mod7(floor(p13 * K)) * K - Ko;
    float3 oz13 = floor(p13 * K2) * Kz - Kzo;

    float3 ox21 = frac(p21 * K) - Ko;
    float3 oy21 = mod7(floor(p21 * K)) * K - Ko;
    float3 oz21 = floor(p21 * K2) * Kz - Kzo;

    float3 ox22 = frac(p22 * K) - Ko;
    float3 oy22 = mod7(floor(p22 * K)) * K - Ko;
    float3 oz22 = floor(p22 * K2) * Kz - Kzo;

    float3 ox23 = frac(p23 * K) - Ko;
    float3 oy23 = mod7(floor(p23 * K)) * K - Ko;
    float3 oz23 = floor(p23 * K2) * Kz - Kzo;

    float3 ox31 = frac(p31 * K) - Ko;
    float3 oy31 = mod7(floor(p31 * K)) * K - Ko;
    float3 oz31 = floor(p31 * K2) * Kz - Kzo;

    float3 ox32 = frac(p32 * K) - Ko;
    float3 oy32 = mod7(floor(p32 * K)) * K - Ko;
    float3 oz32 = floor(p32 * K2) * Kz - Kzo;

    float3 ox33 = frac(p33 * K) - Ko;
    float3 oy33 = mod7(floor(p33 * K)) * K - Ko;
    float3 oz33 = floor(p33 * K2) * Kz - Kzo;

    float3 dx11 = Pfx + jitter * ox11;
    float3 dy11 = Pfy.x + jitter * oy11;
    float3 dz11 = Pfz.x + jitter * oz11;

    float3 dx12 = Pfx + jitter * ox12;
    float3 dy12 = Pfy.x + jitter * oy12;
    float3 dz12 = Pfz.y + jitter * oz12;

    float3 dx13 = Pfx + jitter * ox13;
    float3 dy13 = Pfy.x + jitter * oy13;
    float3 dz13 = Pfz.z + jitter * oz13;

    float3 dx21 = Pfx + jitter * ox21;
    float3 dy21 = Pfy.y + jitter * oy21;
    float3 dz21 = Pfz.x + jitter * oz21;

    float3 dx22 = Pfx + jitter * ox22;
    float3 dy22 = Pfy.y + jitter * oy22;
    float3 dz22 = Pfz.y + jitter * oz22;

    float3 dx23 = Pfx + jitter * ox23;
    float3 dy23 = Pfy.y + jitter * oy23;
    float3 dz23 = Pfz.z + jitter * oz23;

    float3 dx31 = Pfx + jitter * ox31;
    float3 dy31 = Pfy.z + jitter * oy31;
    float3 dz31 = Pfz.x + jitter * oz31;

    float3 dx32 = Pfx + jitter * ox32;
    float3 dy32 = Pfy.z + jitter * oy32;
    float3 dz32 = Pfz.y + jitter * oz32;

    float3 dx33 = Pfx + jitter * ox33;
    float3 dy33 = Pfy.z + jitter * oy33;
    float3 dz33 = Pfz.z + jitter * oz33;

    float3 d11 = dx11 * dx11 + dy11 * dy11 + dz11 * dz11;
    float3 d12 = dx12 * dx12 + dy12 * dy12 + dz12 * dz12;
    float3 d13 = dx13 * dx13 + dy13 * dy13 + dz13 * dz13;
    float3 d21 = dx21 * dx21 + dy21 * dy21 + dz21 * dz21;
    float3 d22 = dx22 * dx22 + dy22 * dy22 + dz22 * dz22;
    float3 d23 = dx23 * dx23 + dy23 * dy23 + dz23 * dz23;
    float3 d31 = dx31 * dx31 + dy31 * dy31 + dz31 * dz31;
    float3 d32 = dx32 * dx32 + dy32 * dy32 + dz32 * dz32;
    float3 d33 = dx33 * dx33 + dy33 * dy33 + dz33 * dz33;

    // Sort out the two smallest distances (F1, F2)
#if 0
    // Cheat and sort out only F1
    float3 d1 = min(min(d11, d12), d13);
    float3 d2 = min(min(d21, d22), d23);
    float3 d3 = min(min(d31, d32), d33);
    float3 d  = min(min(d1, d2), d3);
    d.x       = min(min(d.x, d.y), d.z);
    return sqrt(d.x).xx; // F1 duplicated, no F2 computed
#else
    // Do it right and sort out both F1 and F2
    float3 d1a = min(d11, d12);
    d12        = max(d11, d12);
    d11        = min(d1a, d13); // Smallest now not in d12 or d13
    d13        = max(d1a, d13);
    d12        = min(d12, d13); // 2nd smallest now not in d13
    float3 d2a = min(d21, d22);
    d22        = max(d21, d22);
    d21        = min(d2a, d23); // Smallest now not in d22 or d23
    d23        = max(d2a, d23);
    d22        = min(d22, d23); // 2nd smallest now not in d23
    float3 d3a = min(d31, d32);
    d32        = max(d31, d32);
    d31        = min(d3a, d33); // Smallest now not in d32 or d33
    d33        = max(d3a, d33);
    d32        = min(d32, d33); // 2nd smallest now not in d33
    float3 da  = min(d11, d21);
    d21        = max(d11, d21);
    d11        = min(da, d31); // Smallest now in d11
    d31        = max(da, d31); // 2nd smallest now not in d31
    d11.xy     = (d11.x < d11.y) ? d11.xy : d11.yx;
    d11.xz     = (d11.x < d11.z) ? d11.xz : d11.zx; // d11.x now smallest
    d12        = min(d12, d21); // 2nd smallest now not in d21
    d12        = min(d12, d22); // nor in d22
    d12        = min(d12, d31); // nor in d31
    d12        = min(d12, d32); // nor in d32
    d11.yz     = min(d11.yz, d12.xy); // nor in d12.yz
    d11.y      = min(d11.y, d12.z); // Only two more to go
    d11.y      = min(d11.y, d11.z); // Done! (Phew!)
    return sqrt(d11.xy); // F1, F2
#endif
}

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
    dNoise *= sign(dNoise.w);

    // dNoise.xyz = 2.0 * dNoise.w * dNoise.xyz;
    // dNoise.w   = dNoise.w * dNoise.w;
    // dNoise = dNoise * 2.0 + float4(0.0, 0.0, 0.0, -1.0);
    return dNoise;
}

float4 Ridged(float4 dNoise)
{
    dNoise.xyz *= -sign(dNoise.w);
    dNoise.w = 1.0 - abs(dNoise.w);

    // dNoise.xyz = 2.0 * (dNoise.w + -sign(dNoise.w)) * dNoise.xyz;
    // float r    = 1.0 - abs(dNoise.w);
    // dNoise.w   = r * r;

    // dNoise.xyz = -2.0 * dNoise.w * dNoise.xyz;
    // dNoise.w   = 1.0 - dNoise.w * dNoise.w;
    // dNoise = dNoise * 2.0 + float4(0.0, 0.0, 0.0, -1.0);
    return dNoise;
}

float4 UberNoiseFbm(float3 unitSphere, int numOctaves = 8)
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
    float4 currPerturb     = 0.0;

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
		currPerturb.xyz = SimplexGradNoise(perturbSph * perturbFreq, perturbNoiseSeed).xyz;
        currPerturb.xyz *= lerp(perturb.x, perturb.y, SimplexNoise01(perturbSph * perturbFreq, float4(0.0, 0.0, 0.0, 0.0)));

        sharpnessFreq *= sharpnessLacunarity;
        slopeErosionFreq *= slopeErosionLacunarity;
        perturbFreq *= perturbLacunarity;

        float3 v = featureSph * featureFreq;
        if (amp < 1e-5)
        {
            break;
        }

        float4 gradNoise = SimplexGradNoise(v, featureNoiseSeed);
        float4 billow    = Billowy(gradNoise);
        float4 ridge     = Ridged(gradNoise);

        gradNoise = gradNoise * 0.5 + float4(0.0, 0.0, 0.0, 0.5);
        slopeErosionGrad += gradNoise.xyz * currSlopeErosion;
        currSharpness = i == 0 ? 0.0 : currSharpness;
        gradNoise = lerp(gradNoise, ridge, max(0.0, currSharpness));
        gradNoise = lerp(gradNoise, billow, abs(min(0.0, currSharpness)));

        dampAmp *= 1.0 / (1.0 + dot(slopeErosionGrad, slopeErosionGrad));

        noise += dampAmp * gradNoise.w;
        grad += dampAmp * gradNoise.xyz;
        featureFreq *= lacunarity;
        amp *= gain;
        // amp *= lerp(1.0, smoothstep(0.0, 1.0, sum), altitudeErosion);
        dampAmp = amp;
    }

    return float4(grad, noise);
}

#endif
