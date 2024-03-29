#pragma once
#define NOMINMAX
#include <glm/glm.hpp>

namespace Ashima
{
    //
    // vec3  psrdnoise(vec2 pos, vec2 per, float rot)
    // vec3  psdnoise(vec2 pos, vec2 per)
    // float psrnoise(vec2 pos, vec2 per, float rot)
    // float psnoise(vec2 pos, vec2 per)
    // vec3  srdnoise(vec2 pos, float rot)
    // vec3  sdnoise(vec2 pos)
    // float srnoise(vec2 pos, float rot)
    // float snoise(vec2 pos)
    //
    // Periodic (tiling) 2-D simplex noise (hexagonal lattice gradient noise)
    // with rotating gradients and analytic derivatives.
    // Variants also without the derivative (no "d" in the name), without
    // the tiling property (no "p" in the name) and without the rotating
    // gradients (no "r" in the name).
    //
    // This is (yet) another variation on simplex noise. It's similar to the
    // version presented by Ken Perlin, but the grid is axis-aligned and
    // slightly stretched in the y direction to permit rectangular tiling.
    //
    // The noise can be made to tile seamlessly to any integer period in x and
    // any even integer period in y. Odd periods may be specified for y, but
    // then the actual tiling period will be twice that number.
    //
    // The rotating gradients give the appearance of a swirling motion, and can
    // serve a similar purpose for animation as motion along z in 3-D noise.
    // The rotating gradients in conjunction with the analytic derivatives
    // can make "flow noise" effects as presented by Perlin and Neyret.
    //
    // vec3 {p}s{r}dnoise(vec2 pos {, vec2 per} {, float rot})
    // "pos" is the input (x,y) coordinate
    // "per" is the x and y period, where per.x is a positive integer
    //    and per.y is a positive even integer
    // "rot" is the angle to rotate the gradients (any float value,
    //    where 0.0 is no rotation and 1.0 is one full turn)
    // The first component of the 3-element return vector is the noise value.
    // The second and third components are the x and y partial derivatives.
    //
    // float {p}s{r}noise(vec2 pos {, vec2 per} {, float rot})
    // "pos" is the input (x,y) coordinate
    // "per" is the x and y period, where per.x is a positive integer
    //    and per.y is a positive even integer
    // "rot" is the angle to rotate the gradients (any float value,
    //    where 0.0 is no rotation and 1.0 is one full turn)
    // The return value is the noise value.
    // Partial derivatives are not computed, making these functions faster.
    //
    // Author: Stefan Gustavson (stefan.gustavson@gmail.com)
    // Version 2016-05-10.
    //
    // Many thanks to Ian McEwan of Ashima Arts for the
    // idea of using a permutation polynomial.
    //
    // Copyright (c) 2016 Stefan Gustavson. All rights reserved.
    // Distributed under the MIT license. See LICENSE file.
    // https://github.com/stegu/webgl-noise
    //

    //
    // TODO: One-pixel wide artefacts used to occur due to precision issues with
    // the gradient indexing. This is specific to this variant of noise, because
    // one axis of the simplex grid is perfectly aligned with the input x axis.
    // The errors were rare, and they are now very unlikely to ever be visible
    // after a quick fix was introduced: a small offset is added to the y coordinate.
    // A proper fix would involve using round() instead of floor() in selected
    // places, but the quick fix works fine.
    // (If you run into problems with this, please let me know.)
    //

    inline float mod289(const float x)
    {
        return x - floor(x * (1.0f / 289.0f)) * 289.0f;
    }

    inline float permute(const float x)
    {
        return mod289((x * 34.0f) + 10.0f) * x;
    }

    // Modulo 289, optimizes to code without divisions
    inline glm::vec3 mod289(const glm::vec3 x)
    {
        return x - floor(x * (1.0f / 289.0f)) * 289.0f;
    }

    // Permutation polynomial (ring size 289 = 17*17)
    inline glm::vec3 permute(const glm::vec3 x)
    {
        return mod289(((x * 34.0f) + 10.0f) * x);
    }

    // Hashed 2-D gradients with an extra rotation.
    // (The constant 0.0243902439 is 1/41)
    inline glm::vec2 rgrad2(glm::vec2 p, float rot)
    {
#if 0
        // Map from a line to a diamond such that a shift maps to a rotation.
        float u = permute(permute(p.x) + p.y) * 0.0243902439f + rot; // Rotate by shift
        u       = 4.0f * glm::fract(u) - 2.0f;
        // (This vector could be normalized, exactly or approximately.)
        return { abs(u) - 1.0f, abs(abs(u + 1.0f) - 2.0f) - 1.0f };
#else
        // For more isotropic gradients, sin/cos can be used instead.
        float u = permute(permute(p.x) + p.y) * 0.0243902439f + rot; // Rotate by shift
        u       = glm::fract(u) * 6.28318530718f; // 2*pi
        return { cos(u), sin(u) };
#endif
    }

    //
    // 2-D tiling simplex noise with rotating gradients and analytical derivative.
    // The first component of the 3-element return vector is the noise value,
    // and the second and third components are the x and y partial derivatives.
    //
    inline glm::vec3 psrdnoise(glm::vec2 pos, glm::vec2 per, float rot)
    {
        // Hack: offset y slightly to hide some rare artifacts
        pos.y += 0.01f;
        // Skew to hexagonal grid
        glm::vec2 uv = glm::vec2(pos.x + pos.y * 0.5f, pos.y);

        glm::vec2 i0 = floor(uv);
        glm::vec2 f0 = fract(uv);
        // Traversal order
        glm::vec2 i1 = (f0.x > f0.y) ? glm::vec2(1.0, 0.0) : glm::vec2(0.0f, 1.0f);

        // Unskewed grid points in (x,y) space
        glm::vec2 p0 = glm::vec2(i0.x - i0.y * 0.5f, i0.y);
        glm::vec2 p1 = glm::vec2(p0.x + i1.x - i1.y * 0.5, p0.y + i1.y);
        glm::vec2 p2 = glm::vec2(p0.x + 0.5f, p0.y + 1.0f);

        // Integer grid point indices in (u,v) space
        i1           = i0 + i1;
        glm::vec2 i2 = i0 + glm::vec2(1.0f, 1.0f);

        // Vectors in unskewed (x,y) coordinates from
        // each of the simplex corners to the evaluation point
        glm::vec2 d0 = pos - p0;
        glm::vec2 d1 = pos - p1;
        glm::vec2 d2 = pos - p2;

        // Wrap i0, i1 and i2 to the desired period before gradient hashing:
        // wrap points in (x,y), map to (u,v)
        glm::vec3 xw  = mod(glm::vec3(p0.x, p1.x, p2.x), per.x);
        glm::vec3 yw  = mod(glm::vec3(p0.y, p1.y, p2.y), per.y);
        glm::vec3 iuw = xw + 0.5f * yw;
        glm::vec3 ivw = yw;

        // Create gradients from indices
        glm::vec2 g0 = rgrad2(glm::vec2(iuw.x, ivw.x), rot);
        glm::vec2 g1 = rgrad2(glm::vec2(iuw.y, ivw.y), rot);
        glm::vec2 g2 = rgrad2(glm::vec2(iuw.z, ivw.z), rot);

        // Gradients dot vectors to corresponding corners
        // (The derivatives of this are simply the gradients)
        glm::vec3 w = glm::vec3(dot(g0, d0), dot(g1, d1), dot(g2, d2));

        // Radial weights from corners
        // 0.8 is the square of 2/sqrt(5), the distance from
        // a grid point to the nearest simplex boundary
        glm::vec3 t = 0.8f - glm::vec3(dot(d0, d0), dot(d1, d1), dot(d2, d2));

        // Partial derivatives for analytical gradient computation
        glm::vec3 dtdx = -2.0f * glm::vec3(d0.x, d1.x, d2.x);
        glm::vec3 dtdy = -2.0f * glm::vec3(d0.y, d1.y, d2.y);

        // Set influence of each surflet to zero outside radius sqrt(0.8)
        if (t.x < 0.0f)
        {
            dtdx.x = 0.0f;
            dtdy.x = 0.0f;
            t.x    = 0.0f;
        }
        if (t.y < 0.0f)
        {
            dtdx.y = 0.0f;
            dtdy.y = 0.0f;
            t.y    = 0.0f;
        }
        if (t.z < 0.0f)
        {
            dtdx.z = 0.0f;
            dtdy.z = 0.0f;
            t.z    = 0.0f;
        }

        // Fourth power of t (and third power for derivative)
        glm::vec3 t2 = t * t;
        glm::vec3 t4 = t2 * t2;
        glm::vec3 t3 = t2 * t;

        // Final noise value is:
        // sum of ((radial weights) times (gradient dot vector from corner))
        float n = dot(t4, w);

        // Final analytical derivative (gradient of a sum of scalar products)
        glm::vec2 dt0 = glm::vec2(dtdx.x, dtdy.x) * 4.0f * t3.x;
        glm::vec2 dn0 = t4.x * g0 + dt0 * w.x;
        glm::vec2 dt1 = glm::vec2(dtdx.y, dtdy.y) * 4.0f * t3.y;
        glm::vec2 dn1 = t4.y * g1 + dt1 * w.y;
        glm::vec2 dt2 = glm::vec2(dtdx.z, dtdy.z) * 4.0f * t3.z;
        glm::vec2 dn2 = t4.z * g2 + dt2 * w.z;

        return 11.0f * glm::vec3(n, dn0 + dn1 + dn2);
    }

    //
    // 2-D tiling simplex noise with fixed gradients
    // and analytical derivative.
    // This function is implemented as a wrapper to "psrdnoise",
    // at the minimal cost of three extra additions.
    //
    inline glm::vec3 psdnoise(const glm::vec2 pos, const glm::vec2 per)
    {
        return psrdnoise(pos, per, 0.0);
    }

    //
    // 2-D tiling simplex noise with rotating gradients,
    // but without the analytical derivative.
    //
    inline float psrnoise(glm::vec2 pos, glm::vec2 per, float rot)
    {
        // Offset y slightly to hide some rare artifacts
        pos.y += 0.001f;
        // Skew to hexagonal grid
        glm::vec2 uv = glm::vec2(pos.x + pos.y * 0.5, pos.y);

        glm::vec2 i0 = floor(uv);
        glm::vec2 f0 = fract(uv);
        // Traversal order
        glm::vec2 i1 = (f0.x > f0.y) ? glm::vec2(1.0, 0.0) : glm::vec2(0.0, 1.0);

        // Unskewed grid points in (x,y) space
        glm::vec2 p0 = glm::vec2(i0.x - i0.y * 0.5, i0.y);
        glm::vec2 p1 = glm::vec2(p0.x + i1.x - i1.y * 0.5, p0.y + i1.y);
        glm::vec2 p2 = glm::vec2(p0.x + 0.5, p0.y + 1.0);

        // Integer grid point indices in (u,v) space
        i1           = i0 + i1;
        glm::vec2 i2 = i0 + glm::vec2(1.0, 1.0);

        // Vectors in unskewed (x,y) coordinates from
        // each of the simplex corners to the evaluation point
        glm::vec2 d0 = pos - p0;
        glm::vec2 d1 = pos - p1;
        glm::vec2 d2 = pos - p2;

        // Wrap i0, i1 and i2 to the desired period before gradient hashing:
        // wrap points in (x,y), map to (u,v)
        glm::vec3 xw  = mod(glm::vec3(p0.x, p1.x, p2.x), per.x);
        glm::vec3 yw  = mod(glm::vec3(p0.y, p1.y, p2.y), per.y);
        glm::vec3 iuw = xw + 0.5f * yw;
        glm::vec3 ivw = yw;

        // Create gradients from indices
        glm::vec2 g0 = rgrad2(glm::vec2(iuw.x, ivw.x), rot);
        glm::vec2 g1 = rgrad2(glm::vec2(iuw.y, ivw.y), rot);
        glm::vec2 g2 = rgrad2(glm::vec2(iuw.z, ivw.z), rot);

        // Gradients dot vectors to corresponding corners
        // (The derivatives of this are simply the gradients)
        glm::vec3 w = glm::vec3(dot(g0, d0), dot(g1, d1), dot(g2, d2));

        // Radial weights from corners
        // 0.8 is the square of 2/sqrt(5), the distance from
        // a grid point to the nearest simplex boundary
        glm::vec3 t = 0.8f - glm::vec3(dot(d0, d0), dot(d1, d1), dot(d2, d2));

        // Set influence of each surflet to zero outside radius sqrt(0.8)
        t = max(t, 0.0f);

        // Fourth power of t
        glm::vec3 t2 = t * t;
        glm::vec3 t4 = t2 * t2;

        // Final noise value is:
        // sum of ((radial weights) times (gradient dot vector from corner))
        float n = dot(t4, w);

        // Rescale to cover the range [-1,1] reasonably well
        return 11.0f * n;
    }

    //
    // 2-D tiling simplex noise with fixed gradients,
    // without the analytical derivative.
    // This function is implemented as a wrapper to "psrnoise",
    // at the minimal cost of three extra additions.
    //
    inline float psnoise(const glm::vec2 pos, const glm::vec2 per)
    {
        return psrnoise(pos, per, 0.0f);
    }

    //
    // 2-D non-tiling simplex noise with rotating gradients and analytical derivative.
    // The first component of the 3-element return vector is the noise value,
    // and the second and third components are the x and y partial derivatives.
    //
    inline glm::vec3 srdnoise(glm::vec2 pos, float rot)
    {
        // Offset y slightly to hide some rare artifacts
        pos.y += 0.001f;
        // Skew to hexagonal grid
        glm::vec2 uv = glm::vec2(pos.x + pos.y * 0.5f, pos.y);

        glm::vec2 i0 = floor(uv);
        glm::vec2 f0 = fract(uv);
        // Traversal order
        glm::vec2 i1 = (f0.x > f0.y) ? glm::vec2(1.0f, 0.0f) : glm::vec2(0.0f, 1.0f);

        // Unskewed grid points in (x,y) space
        glm::vec2 p0 = glm::vec2(i0.x - i0.y * 0.5f, i0.y);
        glm::vec2 p1 = glm::vec2(p0.x + i1.x - i1.y * 0.5f, p0.y + i1.y);
        glm::vec2 p2 = glm::vec2(p0.x + 0.5f, p0.y + 1.0f);

        // Integer grid point indices in (u,v) space
        i1           = i0 + i1;
        glm::vec2 i2 = i0 + glm::vec2(1.0f, 1.0f);

        // Vectors in unskewed (x,y) coordinates from
        // each of the simplex corners to the evaluation point
        glm::vec2 d0 = pos - p0;
        glm::vec2 d1 = pos - p1;
        glm::vec2 d2 = pos - p2;

        glm::vec3 x   = glm::vec3(p0.x, p1.x, p2.x);
        glm::vec3 y   = glm::vec3(p0.y, p1.y, p2.y);
        glm::vec3 iuw = x + 0.5f * y;
        glm::vec3 ivw = y;

        // Avoid precision issues in permutation
        iuw = mod289(iuw);
        ivw = mod289(ivw);

        // Create gradients from indices
        glm::vec2 g0 = rgrad2(glm::vec2(iuw.x, ivw.x), rot);
        glm::vec2 g1 = rgrad2(glm::vec2(iuw.y, ivw.y), rot);
        glm::vec2 g2 = rgrad2(glm::vec2(iuw.z, ivw.z), rot);

        // Gradients dot vectors to corresponding corners
        // (The derivatives of this are simply the gradients)
        glm::vec3 w = glm::vec3(dot(g0, d0), dot(g1, d1), dot(g2, d2));

        // Radial weights from corners
        // 0.8 is the square of 2/sqrt(5), the distance from
        // a grid point to the nearest simplex boundary
        glm::vec3 t = 0.8f - glm::vec3(dot(d0, d0), dot(d1, d1), dot(d2, d2));

        // Partial derivatives for analytical gradient computation
        glm::vec3 dtdx = -2.0f * glm::vec3(d0.x, d1.x, d2.x);
        glm::vec3 dtdy = -2.0f * glm::vec3(d0.y, d1.y, d2.y);

        // Set influence of each surflet to zero outside radius sqrt(0.8)
        if (t.x < 0.0f)
        {
            dtdx.x = 0.0f;
            dtdy.x = 0.0f;
            t.x    = 0.0f;
        }
        if (t.y < 0.0f)
        {
            dtdx.y = 0.0f;
            dtdy.y = 0.0f;
            t.y    = 0.0f;
        }
        if (t.z < 0.0f)
        {
            dtdx.z = 0.0f;
            dtdy.z = 0.0f;
            t.z    = 0.0f;
        }

        // Fourth power of t (and third power for derivative)
        glm::vec3 t2 = t * t;
        glm::vec3 t4 = t2 * t2;
        glm::vec3 t3 = t2 * t;

        // Final noise value is:
        // sum of ((radial weights) times (gradient dot vector from corner))
        float n = dot(t4, w);

        // Final analytical derivative (gradient of a sum of scalar products)
        glm::vec2 dt0 = glm::vec2(dtdx.x, dtdy.x) * 4.0f * t3.x;
        glm::vec2 dn0 = t4.x * g0 + dt0 * w.x;
        glm::vec2 dt1 = glm::vec2(dtdx.y, dtdy.y) * 4.0f * t3.y;
        glm::vec2 dn1 = t4.y * g1 + dt1 * w.y;
        glm::vec2 dt2 = glm::vec2(dtdx.z, dtdy.z) * 4.0f * t3.z;
        glm::vec2 dn2 = t4.z * g2 + dt2 * w.z;

        return 11.0f * glm::vec3(n, dn0 + dn1 + dn2);
    }

    //
    // 2-D non-tiling simplex noise with fixed gradients and analytical derivative.
    // This function is implemented as a wrapper to "srdnoise",
    // at the minimal cost of three extra additions.
    //
    inline glm::vec3 sdnoise(const glm::vec2 pos)
    {
        return srdnoise(pos, 0.0f);
    }

    //
    // 2-D non-tiling simplex noise with rotating gradients,
    // without the analytical derivative.
    //
    inline float srnoise(glm::vec2 pos, float rot)
    {
        // Offset y slightly to hide some rare artifacts
        pos.y += 0.001f;
        // Skew to hexagonal grid
        glm::vec2 uv = glm::vec2(pos.x + pos.y * 0.5f, pos.y);

        glm::vec2 i0 = floor(uv);
        glm::vec2 f0 = fract(uv);
        // Traversal order
        glm::vec2 i1 = (f0.x > f0.y) ? glm::vec2(1.0f, 0.0f) : glm::vec2(0.0f, 1.0f);

        // Unskewed grid points in (x,y) space
        glm::vec2 p0 = glm::vec2(i0.x - i0.y * 0.5f, i0.y);
        glm::vec2 p1 = glm::vec2(p0.x + i1.x - i1.y * 0.5f, p0.y + i1.y);
        glm::vec2 p2 = glm::vec2(p0.x + 0.5f, p0.y + 1.0f);

        // Integer grid point indices in (u,v) space
        i1           = i0 + i1;
        glm::vec2 i2 = i0 + glm::vec2(1.0f, 1.0f);

        // Vectors in unskewed (x,y) coordinates from
        // each of the simplex corners to the evaluation point
        glm::vec2 d0 = pos - p0;
        glm::vec2 d1 = pos - p1;
        glm::vec2 d2 = pos - p2;

        // Wrap i0, i1 and i2 to the desired period before gradient hashing:
        // wrap points in (x,y), map to (u,v)
        glm::vec3 x   = glm::vec3(p0.x, p1.x, p2.x);
        glm::vec3 y   = glm::vec3(p0.y, p1.y, p2.y);
        glm::vec3 iuw = x + 0.5f * y;
        glm::vec3 ivw = y;

        // Avoid precision issues in permutation
        iuw = mod289(iuw);
        ivw = mod289(ivw);

        // Create gradients from indices
        glm::vec2 g0 = rgrad2(glm::vec2(iuw.x, ivw.x), rot);
        glm::vec2 g1 = rgrad2(glm::vec2(iuw.y, ivw.y), rot);
        glm::vec2 g2 = rgrad2(glm::vec2(iuw.z, ivw.z), rot);

        // Gradients dot vectors to corresponding corners
        // (The derivatives of this are simply the gradients)
        glm::vec3 w = glm::vec3(dot(g0, d0), dot(g1, d1), dot(g2, d2));

        // Radial weights from corners
        // 0.8 is the square of 2/sqrt(5), the distance from
        // a grid point to the nearest simplex boundary
        glm::vec3 t = 0.8f - glm::vec3(dot(d0, d0), dot(d1, d1), dot(d2, d2));

        // Set influence of each surflet to zero outside radius sqrt(0.8)
        t = max(t, 0.0f);

        // Fourth power of t
        glm::vec3 t2 = t * t;
        glm::vec3 t4 = t2 * t2;

        // Final noise value is:
        // sum of ((radial weights) times (gradient dot vector from corner))
        float n = dot(t4, w);

        // Rescale to cover the range [-1,1] reasonably well
        return 11.0f * n;
    }

    //
    // 2-D non-tiling simplex noise with fixed gradients,
    // without the analytical derivative.
    // This function is implemented as a wrapper to "srnoise",
    // at the minimal cost of three extra additions.
    // Note: if this kind of noise is all you want, there are faster
    // GLSL implementations of non-tiling simplex noise out there.
    // This one is included mainly for completeness and compatibility
    // with the other functions in the file.
    //
    inline float snoise(const glm::vec2 pos)
    {
        return srnoise(pos, 0.0f);
    }
}
