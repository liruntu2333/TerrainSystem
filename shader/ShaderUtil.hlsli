#ifndef SHADER_UTIL
#define SHADER_UTIL

static const uint PatchScale = 255;
static const float SphereRadius = 200000.0f;
static const float Pi = 3.1415926535897932384626433832795f;

cbuffer PassConstants : register(b0)
{
float4x4 ViewProjection;
float3 LightDirection;
float LightIntensity;
float3 ViewPosition;
float OneOverWidth;
float2 AlphaOffset;
float HeightMapScale;
float AmbientIntensity;
}

float4 LoadColor(const uint col)
{
    float b = (col & 0x000000FF) / 255.0f;
    float g = ((col & 0x0000FF00) >> 8) / 255.0f;
    float r = ((col & 0x00FF0000) >> 16) / 255.0f;
    float a = ((col & 0xFF000000) >> 24) / 255.0f;
    return float4(r, g, b, a);
}

float3 EvalSh(float3 _dir)
{
#define k01 0.2820947918 // sqrt( 1/PI)/2
#define k02 0.4886025119 // sqrt( 3/PI)/2
#define k03 1.0925484306 // sqrt(15/PI)/2
#define k04 0.3153915652 // sqrt( 5/PI)/4
#define k05 0.5462742153 // sqrt(15/PI)/4

    float3 shEnv[9];
    shEnv[0] = float3(0.967757057878229854, 0.976516067990363390, 0.891218272348969998); /* Band 0 */
    shEnv[1] = float3(-0.384163503608655643, -0.423492289131209787, -0.425532726148547868); /* Band 1 */
    shEnv[2] = float3(0.055906294587354334, 0.056627436881069373, 0.069969936396987467);
    shEnv[3] = float3(0.120985157386215209, 0.119297994074027414, 0.117111965829213599);
    shEnv[4] = float3(-0.176711633774331106, -0.170331404095516392, -0.151345020570876621); /* Band 2 */
    shEnv[5] = float3(-0.124682114349692147, -0.119340785411183953, -0.096300354204368860);
    shEnv[6] = float3(0.001852378550138503, -0.032592784164597745, -0.088204495001329680);
    shEnv[7] = float3(0.296365482782109446, 0.281268696656263029, 0.243328223888495510);
    shEnv[8] = float3(-0.079826665303240341, -0.109340956251195970, -0.157208859664677764);

    float3 nn = _dir.zxy;

    float sh[9];
    sh[0] = k01;
    sh[1] = -k02 * nn.y;
    sh[2] = k02 * nn.z;
    sh[3] = -k02 * nn.x;
    sh[4] = k03 * nn.y * nn.x;
    sh[5] = -k03 * nn.y * nn.z;
    sh[6] = k04 * (3.0 * nn.z * nn.z - 1.0);
    sh[7] = -k03 * nn.x * nn.z;
    sh[8] = k05 * (nn.x * nn.x - nn.y * nn.y);

    float3 rgb = 0.0;
    rgb += shEnv[0] * sh[0] * 1.0;
    rgb += shEnv[1] * sh[1] * 2.0 / 2.5;
    rgb += shEnv[2] * sh[2] * 2.0 / 2.5;
    rgb += shEnv[3] * sh[3] * 2.0 / 2.5;
    rgb += shEnv[4] * sh[4] * 1.0 / 2.5;
    rgb += shEnv[5] * sh[5] * 0.5;
    rgb += shEnv[6] * sh[6] * 0.5;
    rgb += shEnv[7] * sh[7] * 0.5;
    rgb += shEnv[8] * sh[8] * 0.5;

    return rgb;
}

float3 Shade(const float3 normal, const float3 lightDir, const float lightIntensity, const float occlusion)
{
    return dot(normal, lightDir) * lightIntensity + occlusion * EvalSh(normal);
}

float3 ToneMapping(const float3 color)
{
    return color / (color + float3(1.0f, 1.0f, 1.0f));
}

float3 GammaCorrect(const float3 color)
{
    return pow(color, 1.0f / 2.2f);
}

float Luminance(const float3 color)
{
    return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

float3 MakeSphere(const float2 xz, const float y, const float3 view, const float r)
{
	const float3 center = float3(view.x, -r, view.z);
    float3 p = center + normalize(float3(xz.x, 0, xz.y) - center) * (r + y);
	return p;
}

float SampleClipmapLevel(
    Texture2DArray<float> tex, const SamplerState pw,
    const float3 uvf, const float3 uvc, const float alpha)
{
    const float fine = tex.SampleLevel(pw, uvf, 0);
    const float coarse = tex.SampleLevel(pw, uvc, 0);
    return lerp(fine, coarse, alpha);
}

float4 SampleClipmapLevel(
    Texture2DArray tex, const SamplerState aw,
    const float3 uvf, const float3 uvc, const float alpha)
{
    float4 fine, coarse;
#ifdef HARDWARE_FILTERING
    fine = tex.Sample(aw, uvf);
    coarse = tex.Sample(aw, uvc);
#else
    fine = tex.SampleLevel(aw, uvf, 0);
    coarse = tex.SampleLevel(aw, uvc, 0);
#endif

    return lerp(fine, coarse, alpha);
}

float DTerm(const float nh, const float roughness)
{
    const float a = roughness * roughness;
    const float a2 = a * a;
    const float d = (nh * a2 - nh) * nh + 1.0;
    return a2 / (Pi * d * d);
}

float GTerm(const float nl, const float nv, const float roughness)
{
    const float a = roughness + 1.0f;
    const float k = (a * a) / 8.0f;
    const float gl = nl / (nl * (1.0 - k) + k);
    const float gv = nv / (nv * (1.0 - k) + k);
    return gl * gv;
}

float Pow5(const float x)
{
    return x * x * x * x * x;
}

float3 FTerm(const float cosTheta, const float3 f0)
{
    return f0 + (1.0f - f0) * Pow5(1.0f - cosTheta);
}

float3 FTermR(const float cosTheta, const float3 f0, const float roughness)
{
    return f0 + (max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), f0) - f0) * Pow5(1.0f - cosTheta);
}

float3 Brdf(
    const float3 l, const float3 v, const float3 n,
    const float3 f0, const float3 alb, const float metallic, const float roughness)
{
    const float3 h = normalize(l + v);
    const float nl = saturate(dot(n, l));
    const float nv = saturate(dot(n, v));
    const float nh = saturate(dot(n, h));
    const float lh = saturate(dot(l, h));
    const float rroughness = max(0.05f, roughness);

    const float3 f = FTerm(lh, f0);
    const float d = DTerm(nh, rroughness);
    const float g = GTerm(nl, nv, rroughness);

    const float3 nominator = f * d * g;
    const float denominator = 4.0f * nl * nv;
    float3 specular = nominator / max(denominator, 0.001f);

    const float3 kD = (1.0f - f) * (1.0f - metallic);
    const float3 diffuse = kD * alb / Pi;
    specular *= 1.0f - kD;

    return diffuse + specular;
}

#endif
